/*
 * This file is part of esip.
 *
 * esip is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * esip is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with esip.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>

#include <event2/event.h>
#include <event2/util.h>

#include "eserror.h"
#include "log.h"

#include "estransport.h"

/** Transport context magic */
#define ES_TRANSPORT_MAGIC            0x20140921

/** Default SIP port */
#define ES_TRANSPORT_DEFAULT_PORT     5060

/** Max packet size to read from network */
#define ES_TRANSPORT_MAX_BUFFER_SIZE  2048 //Baby jumbo frame max frame size

struct es_transport_s {
  /** Magic */
  uint32_t                         magic;
  /** */
  int                              udp_socket;
  /** */
  struct event                     *evudpsock;
  /** */
  struct event_base                *base;
  /** */
  struct es_transport_callbacks_s  callbacks;
};

/**
 * @brief Internal transport event handler
 */
static void _es_transport_ev(evutil_socket_t fd, short event, void *arg);

/**
 * @brief Bind a socket
 * @return ES_OK on success
 * @note Only IPv4 supported
 */
static es_status _es_bind_socket(int sock, const char *ipv4addr, const unsigned int port);

es_status es_transport_init(es_transport_t **pCtx, struct event_base *pBase)
{
  struct es_transport_s * _pCtx = NULL;

  if (pBase == NULL) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Transport Loop not valid");
    return ES_ERROR_NULLPTR;
  }

  _pCtx = (struct es_transport_s *) malloc(sizeof(struct es_transport_s));
  if (_pCtx == NULL) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Initialisation failed");
    return ES_ERROR_OUTOFRESOURCES;
  }

  memset(_pCtx, 0, sizeof(struct es_transport_s));

  /* Set Magic */
  _pCtx->magic = ES_TRANSPORT_MAGIC;

  _pCtx->udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
  if (_pCtx->udp_socket == -1) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Can not create socket");
    free(_pCtx);
    return ES_ERROR_NETWORK_PROBLEM;
  }

  /* Base event loop */
  _pCtx->base = pBase;

  /* Set REUSEADDR ON */
  {
    int on = 1;
    if (setsockopt(_pCtx->udp_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
      ESIP_TRACE(ESIP_LOG_WARNING, "setsockopt returned error");
    }
  }

  /* Set NONBLOCKING */
  evutil_make_socket_nonblocking(_pCtx->udp_socket);

  *pCtx = _pCtx;
  return ES_OK;
}

/**
 * @brief
 * @param pCtx
 * @return
 */
es_status es_transport_destroy(es_transport_t *pCtx)
{
  struct es_transport_s * _pCtx = (struct es_transport_s *) pCtx;

  if (_pCtx == (struct es_transport_s *)0) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Transport Ctx not valid");
    return ES_ERROR_NULLPTR;
  }

  if (_pCtx->magic != ES_TRANSPORT_MAGIC) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Transport Ctx not valid");
    return ES_ERROR_NULLPTR;
  }

  close(_pCtx->udp_socket);

  memset(_pCtx, 0, sizeof(struct es_transport_s));

  free(_pCtx);

  return ES_OK;
}

es_status es_transport_start(es_transport_t *pCtx)
{
  struct es_transport_s * _pCtx = (struct es_transport_s *) pCtx;

  if (_pCtx == (struct es_transport_s *)0) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Transport Ctx not valid");
    return ES_ERROR_NULLPTR;
  }

  if (_pCtx->magic != ES_TRANSPORT_MAGIC) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Transport Ctx not valid");
    return ES_ERROR_NULLPTR;
  }

  if (_es_bind_socket(_pCtx->udp_socket, NULL, ES_TRANSPORT_DEFAULT_PORT) != ES_OK) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Can not bind on socket");
    return ES_ERROR_NETWORK_PROBLEM;
  }

  _pCtx->evudpsock = event_new(pCtx->base, _pCtx->udp_socket, (EV_READ|EV_PERSIST), _es_transport_ev, (void *)_pCtx);
  if (_pCtx->evudpsock == NULL) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Can not create event for socket");
    return ES_ERROR_UNKNOWN;
  }

  /* set priority */
  if (event_priority_set(_pCtx->evudpsock, 0) != 0) {
    ESIP_TRACE(ESIP_LOG_WARNING, "Can not set priority event for socket");
  }

  if (event_add(_pCtx->evudpsock, NULL) != 0) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Can not make socket event pending");
    return ES_ERROR_UNKNOWN;
  }

  return ES_OK;
}

es_status es_transport_stop(es_transport_t *pCtx)
{
  struct es_transport_s * _pCtx = (struct es_transport_s *)pCtx;
  if (_pCtx == (struct es_transport_s *)0) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Transport Ctx not valid");
    return ES_ERROR_NULLPTR;
  }

  if (_pCtx->magic != ES_TRANSPORT_MAGIC) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Transport Ctx not valid");
    return ES_ERROR_NULLPTR;
  }

  if (event_del(_pCtx->evudpsock) != 0) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Delete Event from loop failed.");
  }

  event_free(_pCtx->evudpsock);

  return ES_OK;
}

es_status es_transport_set_callbacks(es_transport_t *pCtx, struct es_transport_callbacks_s *pCbCtx)
{
  struct es_transport_s * _pCtx = (struct es_transport_s *)pCtx;

  if ((pCbCtx == (struct es_transport_callbacks_s *)0) || (_pCtx == (struct es_transport_s *)0)) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Transport Ctx not valid");
    return ES_ERROR_NULLPTR;
  }

  if (_pCtx->magic != ES_TRANSPORT_MAGIC) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Transport Ctx not valid");
    return ES_ERROR_NULLPTR;
  }

  _pCtx->callbacks.event_cb     = pCbCtx->event_cb;
  _pCtx->callbacks.msg_recv_cb  = pCbCtx->msg_recv_cb;
  _pCtx->callbacks.user_data    = pCbCtx->user_data;

  return ES_OK;
}

es_status es_transport_set_dscp(es_transport_t *pCtx, int dscp)
{
  int tos = 0;
  struct es_transport_s * _pCtx = (struct es_transport_s *)pCtx;

  if (_pCtx == (struct es_transport_s *)0) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Transport Ctx not valid");
    return ES_ERROR_NULLPTR;
  }

  if (_pCtx->magic != ES_TRANSPORT_MAGIC) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Transport Ctx not valid");
    return ES_ERROR_NULLPTR;
  }

  if (_pCtx->udp_socket == 0) {
    return ES_ERROR_UNINITIALIZED;
  }

  tos = (dscp << 2) & 0xff;
  if (setsockopt(_pCtx->udp_socket, SOL_IP, IP_TOS, &tos, sizeof(tos))) {
    ESIP_TRACE(ESIP_LOG_ERROR,"setsockopt() failed while setting DSCP value");
    return ES_ERROR_NETWORK_PROBLEM;
  }

  return ES_OK;
}

es_status es_transport_get_udp_socket(es_transport_t *pCtx, int *fd)
{
  struct es_transport_s * _pCtx = (struct es_transport_s *)pCtx;

  if (_pCtx == (struct es_transport_s *)0) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Transport Ctx not valid");
    return ES_ERROR_NULLPTR;
  }

  if (_pCtx->magic != ES_TRANSPORT_MAGIC) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Transport Ctx not valid");
    return ES_ERROR_NULLPTR;
  }

  if (_pCtx->udp_socket > 0) {
    *fd = pCtx->udp_socket;
    return ES_OK;
  }

  return ES_ERROR_UNINITIALIZED;
}

es_status es_transport_send(es_transport_t *pCtx, char *ip, int port, const char *msg, size_t size)
{
  struct sockaddr_in saddr;
  struct es_transport_s *_pCtx = (struct es_transport_s *)pCtx;

  if (_pCtx == (struct es_transport_s *)0) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Transport Ctx not valid");
    return ES_ERROR_NULLPTR;
  }

  if (_pCtx->magic != ES_TRANSPORT_MAGIC) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Transport Ctx not valid");
    return ES_ERROR_NULLPTR;
  }

  saddr.sin_family = AF_INET;
  saddr.sin_addr.s_addr = inet_addr(ip);
  saddr.sin_port = htons(port);

  sendto(_pCtx->udp_socket, msg, size, 0, (const struct sockaddr *)&saddr, sizeof(saddr));

  return ES_OK;
}

static es_status _es_bind_socket(int sock, const char *ipv4addr, const unsigned int port)
{
  struct sockaddr_in addr;

  memset(&addr, 0, sizeof(addr));

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = (ipv4addr == NULL) ? INADDR_ANY : inet_addr(ipv4addr);
  if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Can not bind on socket");
    return ES_ERROR_NETWORK_PROBLEM;
  }

  return ES_OK;
}

static void _es_transport_ev(evutil_socket_t fd, short event, void *arg)
{
  struct es_transport_s * _pCtx = (struct es_transport_s *)arg;

  if (_pCtx == (struct es_transport_s *)0) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Transaction Ctx invalid");
    return;
  }

  if (_pCtx->magic != ES_TRANSPORT_MAGIC) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Transport Ctx not valid");
    return;
  }

  if (event & EV_READ) {
    struct sockaddr_in remote_addr;
    socklen_t len = sizeof(struct sockaddr_in);
    char buf[ES_TRANSPORT_MAX_BUFFER_SIZE];
    size_t buf_len = sizeof(buf);

    buf_len = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&remote_addr, &len);

    buf[buf_len] = '\0';

    ESIP_TRACE(ESIP_LOG_DEBUG, "Packet recieved from %s:%d [Len:%d]",
        inet_ntoa(remote_addr.sin_addr),
        ntohs(remote_addr.sin_port),
        (unsigned int)buf_len);

    if ((buf_len > 0) && (_pCtx->callbacks.msg_recv_cb != NULL)) {
      _pCtx->callbacks.msg_recv_cb(_pCtx, buf, buf_len, _pCtx->callbacks.user_data);
    }

    if (_pCtx->callbacks.event_cb != NULL) {
      _pCtx->callbacks.event_cb(_pCtx, 1, 0, _pCtx->callbacks.user_data);
    }
  }
}
// vim: ts=2:sw=2
