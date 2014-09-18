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

#define ES_TRANSPORT_DEFAULT_PORT     5060
#define ES_TRANSPORT_MAX_BUFFER_SIZE  2048 //Baby jumbo frame max frame size

struct es_transport_s {
   int udp_socket;
   struct event * evudpsock;
   struct event_base * base;

   struct es_transport_callbacks_s callbacks;
};

/**
 * @brief Internal transport event handler
 * @return void
 */
static void _es_transport_ev(
      evutil_socket_t fd,
      short event,
      void * arg);

/**
 * @brief Bind a socket
 * @return ES_OK on success
 * @note Only IPv4 supported
 */
static es_status _es_bind_socket(
      int sock,
      const char * ipv4addr,
      const unsigned int port);

es_status es_transport_init(
      OUT es_transport_t  **  ctx,
      IN  struct event_base  * loop)
{
   struct es_transport_s * _ctx = NULL;

   if (loop == NULL) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Transport Loop not valid");
      return ES_ERROR_NULLPTR;
   }

   _ctx = (struct es_transport_s *) malloc(sizeof(struct es_transport_s));
   if (_ctx == NULL) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Initialisation failed");
      return ES_ERROR_OUTOFRESOURCES;
   }

   memset(_ctx, 0, sizeof(struct es_transport_s));

   _ctx->udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
   if (_ctx->udp_socket == -1) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Can not create socket");
      free(_ctx);
      return ES_ERROR_NETWORK_PROBLEM;
   }

   _ctx->base = loop;

   evutil_make_socket_nonblocking(_ctx->udp_socket);

   *ctx = _ctx;
   return ES_OK;
}

es_status es_transport_destroy(
      IN es_transport_t * ctx)
{
   if (ctx == NULL) {
      return ES_ERROR_NULLPTR;
   }

   close(ctx->udp_socket);

   memset(ctx, 0, sizeof(struct es_transport_s));

   free(ctx);

   return ES_OK;
}

es_status es_transport_start(
      IN es_transport_t * ctx)
{
   struct es_transport_s * _ctx = (struct es_transport_s *) ctx;

   if (ctx == NULL) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Transport Ctx not valid");
      return ES_ERROR_NULLPTR;
   }

   if (_es_bind_socket(_ctx->udp_socket, NULL, ES_TRANSPORT_DEFAULT_PORT) != ES_OK) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Can not bind on socket");
      free(_ctx);
      return ES_ERROR_NETWORK_PROBLEM;
   }

   _ctx->evudpsock = event_new(ctx->base, _ctx->udp_socket,
                               (EV_READ|EV_PERSIST), _es_transport_ev, (void *)_ctx);
   if (_ctx->evudpsock == NULL) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Can not create event for socket");
      free(_ctx);
      return ES_ERROR_UNKNOWN;
   }

   /* set priority */
   if (event_priority_set(_ctx->evudpsock, 0) != 0) {
      ESIP_TRACE(ESIP_LOG_WARNING, "Can not set priority event for socket");
   }

   if (event_add(_ctx->evudpsock, NULL) != 0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Can not make socket event pending");
      free(_ctx);
      return ES_ERROR_UNKNOWN;
   }

   return ES_OK;
}

es_status es_transport_stop(
      IN es_transport_t *pCtx)
{
   struct es_transport_s * _pCtx = (struct es_transport_s *)pCtx;
   if (_pCtx == (struct es_transport_s *)0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Transport Ctx not valid");
      return ES_ERROR_NULLPTR;
   }

   if (event_del(_pCtx->evudpsock) != 0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Delete Event from loop failed.");
   }

   event_free(_pCtx->evudpsock);

   return ES_OK;
}

es_status es_transport_set_callbacks(
      IN es_transport_t * _ctx,
      IN struct es_transport_callbacks_s * cs)
{
   struct es_transport_s * ctx = (struct es_transport_s *)_ctx;

   if ((cs == NULL) || (ctx == NULL)) {
      return ES_ERROR_NULLPTR;
   }

   ctx->callbacks.event_cb   = cs->event_cb;
   ctx->callbacks.msg_recv_cb  = cs->msg_recv_cb;
   ctx->callbacks.user_data  = cs->user_data;

   return ES_OK;
}

es_status es_transport_get_udp_socket(
      IN    es_transport_t * _ctx,
      OUT   int    *   fd)
{
   struct es_transport_s * ctx = (struct es_transport_s *)_ctx;

   if (ctx == NULL) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Transport Ctx not valid");
      return ES_ERROR_NULLPTR;
   }

   if (ctx->udp_socket > 0) {
      *fd = _ctx->udp_socket;
      return ES_OK;
   }

   return ES_ERROR_UNINITIALIZED;
}

es_status es_transport_send(
      IN es_transport_t * ctx,
      IN char * ip,
      IN int port,
      IN const char * msg,
      IN size_t size)
{
   struct sockaddr_in saddr;

   saddr.sin_family = AF_INET;
   saddr.sin_addr.s_addr = inet_addr(ip);
   saddr.sin_port = htons(port);

   sendto(ctx->udp_socket, msg, size, 0, (const struct sockaddr *)&saddr, sizeof(saddr));

   return ES_OK;
}

static es_status _es_bind_socket(
      int sock,
      const char * ipv4addr,
      const unsigned int port)
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

static void _es_transport_ev(
      evutil_socket_t fd,
      short event,
      void * arg)
{
   struct es_transport_s * ctx = (struct es_transport_s *)arg;

   if (ctx == NULL) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Transaction Ctx invalid");
      return;
   }

   if (event & EV_READ) {
      struct sockaddr_in remote_addr;
      socklen_t len = sizeof(struct sockaddr_in);
      char buf[ES_TRANSPORT_MAX_BUFFER_SIZE];
      size_t buf_len = sizeof(buf);

      buf_len = recvfrom(fd, buf, sizeof(buf), 0,
                         (struct sockaddr *)&remote_addr, &len);

      buf[buf_len] = '\0';

      ESIP_TRACE(ESIP_LOG_DEBUG, "Packet recieved from %s:%d [Len: %d]",
                 inet_ntoa(remote_addr.sin_addr),
                 ntohs(remote_addr.sin_port),
                 (unsigned int)buf_len);

      if ((buf_len > 0) && (ctx->callbacks.msg_recv_cb != NULL)) {
         ctx->callbacks.msg_recv_cb(ctx, buf, buf_len, ctx->callbacks.user_data);
      }
      if (ctx->callbacks.event_cb != NULL) {
         ctx->callbacks.event_cb(ctx, 1, 0, ctx->callbacks.user_data);
      }
   }
}
