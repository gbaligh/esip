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

#include <event2/event.h>
#include <event2/util.h>

#include "eserror.h"
#include "log.h"
#include "esosip.h"

/**
 * @brief 
 */
typedef struct app_s {
  struct event_config * cfg;  //!< LibEvent configuration
  struct event_base * base;   //!< LibEvent Base loop
  struct event * evsig;       //!< LibEvent Signal 
  es_log_t * logger;          //!< Logger handler
  es_osip_t * osipCtx;        //!< OSip stack context
} app_t;

static void libevent_log_cb(int severity, const char * msg)
{
  ESIP_TRACE(ESIP_LOG_INFO, "[libevent]- %s", msg);
}

static void signal_cb(evutil_socket_t fd, short event, void * arg)
{
  app_t * ctx = (app_t *)arg;
  ESIP_TRACE(ESIP_LOG_INFO, "Terminate %s", PACKAGE);
  (void)event_base_loopexit(ctx->base, NULL);

  /* TODO: free all allocated mem */
}

int main(const int argc, const char * argv[])
{
  app_t ctx;
  int ret = EXIT_SUCCESS;

  ESIP_TRACE(ESIP_LOG_INFO, "Starting %s v%s", PACKAGE, VERSION);

  event_set_log_callback(libevent_log_cb);

  ctx.cfg = event_config_new();
  if (event_config_avoid_method(ctx.cfg, "select") != 0) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Can not avoid select methode");
  }

  ctx.base = event_base_new_with_config(ctx.cfg);
  if (ctx.base == NULL) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Can not create event Config");
    goto ERROR_EXIT;
  }

  /* free cfg */
  event_config_free(ctx.cfg);
  ctx.cfg = NULL;

  /* Set MAX priority */
  if (event_base_priority_init(ctx.base, 2) != 0) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Can not set MAX priority in event loop");
  }

  ctx.evsig = evsignal_new(ctx.base, SIGINT, &signal_cb, (void *)&ctx);
  if (ctx.evsig == NULL) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Can not create event for signal");
    goto ERROR_EXIT;
  }

  if (evsignal_add(ctx.evsig, NULL) != 0) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Can not make Signal event pending");
    goto ERROR_EXIT;
  }

  /* Init OSip Stack */
  if (es_osip_init(&ctx.osipCtx, ctx.base) != ES_OK) {
    ESIP_TRACE(ESIP_LOG_ERROR, "Can not initialize OSip stack");
    goto ERROR_EXIT;
  }

  ESIP_TRACE(ESIP_LOG_DEBUG, "Starting %s v%s main loop", PACKAGE, VERSION);
  if (event_base_dispatch(ctx.base) != 0) {
    ESIP_TRACE(ESIP_LOG_EMERG, "Can not start main event lopp");
    goto ERROR_EXIT;
  }

  goto EXIT;

ERROR_EXIT:
  ESIP_TRACE(ESIP_LOG_EMERG, "%s fatal error: stoped.", PACKAGE);
  ret = EXIT_FAILURE;

EXIT:
  event_free(ctx.evsig);
  event_base_free(ctx.base);

  return ret;
}

// vim: noai:ts=2:sw=2
