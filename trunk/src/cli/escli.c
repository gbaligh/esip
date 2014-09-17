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

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <event2/event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#include <libcli.h>

#include "eserror.h"
#include "log.h"

#include "escli.h"

#define ES_CLI_MAGIC          0X20140917

#define ES_CLI_BANNER_STR     "######################### " PACKAGE_NAME " commands line #################################"

#define ES_CLI_HOSTNAME_STR   PACKAGE_NAME

#define ES_SERVER_PORT        8008

struct es_cli_s {
   uint32_t                  magic;
   /* base loop thread */
   struct event_base         *base;
   /* */
   struct evconnlistener     *listener;
   /* Cli Context */
   struct cli_def            *cliCtx;
};

struct _es_cli_thd_s {
   struct es_cli_s *pCtx;
   evutil_socket_t fd;
};

#if 0
void _es_bufferevent_read_data_cb(struct bufferevent *bev, void *pCtx)
{
   struct es_cli_s *_pCtx = (struct es_cli_s *)pCtx;

   if (_pCtx == (struct es_cli_s *)0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "NULLPTR for Context");
      return;
   }

   /* Check Magic */
   if (_pCtx->magic != ES_CLI_MAGIC) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Bad Magic: expected(%d) (%d)", ES_CLI_MAGIC, _pCtx->magic);
      return;
   }

   /* This callback is invoked when there is data to read on bev. */
   struct evbuffer *input = bufferevent_get_input(bev);
   struct evbuffer *output = bufferevent_get_output(bev);

   size_t len = evbuffer_get_length(input);
   char *data;
   data = malloc(len);
   evbuffer_copyout(input, data, len);

   printf("we got some data: %s\n", data);

   /* Copy all the data from the input buffer to the output buffer. */
   evbuffer_add_buffer(output, input);
   free(data);
}
#endif

static void * _es_start_cli_thread(void *pCtx)
{
   struct es_cli_s *_pCtx = NULL;
   int fd = -1;
   struct _es_cli_thd_s *pThdCtx = (struct _es_cli_thd_s *)pCtx;
   if (pThdCtx == (struct _es_cli_thd_s *)0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "NULLPTR for Context");
      return NULL;
   }

   _pCtx = (struct es_cli_s *)pThdCtx->pCtx;
   if (_pCtx == (struct es_cli_s *)0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "NULLPTR for Context");
      return NULL;
   }
   fd = pThdCtx->fd;
   if (fd == -1) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Socket descriprot is not valid");
      return NULL;
   }

   if (cli_loop(_pCtx->cliCtx, fd) != 0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Can not start CLI for new connection");
      return NULL;
   }

   free(pThdCtx);
   pThdCtx = NULL;

   return 0;
}

static void _es_evconnlistener_cb(struct evconnlistener *pListner, evutil_socket_t fd, struct sockaddr *pSock, int sockLen, void *pCtx)
{
   ESIP_TRACE(ESIP_LOG_DEBUG, "Enter");
   struct es_cli_s *_pCtx = (struct es_cli_s *)pCtx;

   if (_pCtx == (struct es_cli_s *)0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "NULLPTR for Context");
      return;
   }

   /* Check Magic */
   if (_pCtx->magic != ES_CLI_MAGIC) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Bad Magic: expected(%d) (%d)", ES_CLI_MAGIC, _pCtx->magic);
      return;
   }

   /* TODO: stop listner for this connection and start cli_loop in another thread */
   {
      pthread_t threadNum;
      struct _es_cli_thd_s *pThdCtx = (struct _es_cli_thd_s *) malloc(sizeof(struct _es_cli_thd_s));
      if (pThdCtx == (struct _es_cli_thd_s *)0) {
         ESIP_TRACE(ESIP_LOG_ERROR, "Can not create thread context");
         return;
      }

      pThdCtx->pCtx = _pCtx;
      pThdCtx->fd = fd;

      if (pthread_create(&threadNum, NULL, &_es_start_cli_thread, pThdCtx) != 0) {
         ESIP_TRACE(ESIP_LOG_ERROR, "pthread_create");
         return;
      }
   }

#if 0
   {
      /* We got a new connection! Set up a bufferevent for it. */
      struct bufferevent *bev = bufferevent_socket_new(_pCtx->base, fd, BEV_OPT_CLOSE_ON_FREE);
      if (bev == (struct bufferevent *)0) {
         ESIP_TRACE(ESIP_LOG_ERROR, "Can not allocate new buffer Event");
         return;
      }

      bufferevent_setcb(bev,
                        _es_bufferevent_read_data_cb,  /* Read Callback  */
                        NULL,  /* Write Callback */
                        NULL,  /* Event Callback */
                        (void *) _pCtx); /* Reference */

      if (bufferevent_enable(bev, EV_READ | EV_WRITE) != 0) {
         ESIP_TRACE(ESIP_LOG_ERROR, "Enabling Socket");
         return;
      }
   }
#endif
}

static void _es_accept_error_cb(struct evconnlistener *pListener, void *pCtx)
{
   int err = EVUTIL_SOCKET_ERROR();
   struct es_cli_s *_pCtx = (struct es_cli_s *)pCtx;

   if (_pCtx == (struct es_cli_s *)0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "");
      return;
   }
   /* Check Magic */
   if (_pCtx->magic != ES_CLI_MAGIC) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Bad Magic: expected(%d) (%d)", ES_CLI_MAGIC, _pCtx->magic);
      return;
   }

    ESIP_TRACE(ESIP_LOG_ERROR, "Got an error %d (%s) on the listener. "
               "Shutting down.\n", err, evutil_socket_error_to_string(err));
}

static int _es_cli_regular_callback(struct cli_def *pCliCtx)
{
   if (pCliCtx == (struct cli_def *)0) {
      return ES_ERROR_INVALID_HANDLE;
   }
   cli_print(pCliCtx, "Regular callback");
   cli_reprompt(pCliCtx);
   return 0;
}

static int _es_cli_timeout_callback(struct cli_def *pCliCtx)
{
   if (pCliCtx == (struct cli_def *)0) {
      return ES_ERROR_INVALID_HANDLE;
   }
   cli_print(pCliCtx, "Close connection for no activity.");
   return CLI_QUIT;
}

es_status es_cli_init(es_cli_t           **ppCtx,
                       struct event_base  *pBase)
{
   struct es_cli_s *_pCtx = (struct es_cli_s *) malloc(sizeof(struct es_cli_s));
   if (_pCtx == (struct es_cli_s *)0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "");
      return ES_ERROR_OUTOFRESOURCES;
   }

   if (pBase == (struct event_base *)0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "");
      return ES_ERROR_NULLPTR;
   }

   _pCtx->magic = ES_CLI_MAGIC;
   _pCtx->base = pBase;

   /* Init Cli */
   {
      _pCtx->cliCtx = cli_init();
      if (_pCtx->cliCtx == NULL) {
         ESIP_TRACE(ESIP_LOG_ERROR, "");
         free(_pCtx);
         return ES_ERROR_OUTOFRESOURCES;
      }

      cli_set_banner(_pCtx->cliCtx, ES_CLI_BANNER_STR);
      cli_set_hostname(_pCtx->cliCtx, ES_CLI_HOSTNAME_STR);
      cli_regular(_pCtx->cliCtx, _es_cli_regular_callback);
      cli_regular_interval(_pCtx->cliCtx, 10);
      cli_set_idle_timeout_callback(_pCtx->cliCtx, 30, _es_cli_timeout_callback);
   }

   /* Init Socket and Listener */
   {
      struct sockaddr_in listen_addr;
      /* Create our listening socket. */
      int listenfd = socket(AF_INET, SOCK_STREAM, 0);
      if (listenfd < 0) {
         ESIP_TRACE(ESIP_LOG_ERROR, "");
         free(_pCtx);
         return ES_ERROR_NETWORK_PROBLEM;
      }
      memset(&listen_addr, 0, sizeof(listen_addr));
      listen_addr.sin_family = AF_INET;
      listen_addr.sin_addr.s_addr = INADDR_ANY;
      listen_addr.sin_port = htons(ES_SERVER_PORT);

      /* Create a new listener */
      _pCtx->listener = evconnlistener_new_bind(_pCtx->base, _es_evconnlistener_cb, _pCtx,
                                                LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE, -1,
                                                (struct sockaddr *) &listen_addr, sizeof(listen_addr));
      if (!_pCtx->listener) {
         ESIP_TRACE(ESIP_LOG_ERROR, "");
         close(listenfd);
         free(_pCtx);
         return ES_ERROR_UNKNOWN;
      }

      evconnlistener_disable(_pCtx->listener);

      evconnlistener_set_error_cb(_pCtx->listener, _es_accept_error_cb);
   }


   *ppCtx = _pCtx;
   return ES_OK;
}

es_status es_cli_start(es_cli_t *pCtx)
{
   struct es_cli_s *_pCtx = (struct es_cli_s *)pCtx;
   if (_pCtx == (struct es_cli_s *)0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "");
      return ES_ERROR_NULLPTR;
   }

   ESIP_TRACE(ESIP_LOG_INFO, "Starting CLI for ESip");
   evconnlistener_enable(_pCtx->listener);

   return ES_OK;
}
