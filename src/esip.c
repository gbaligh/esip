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
#include <sys/signal.h>
#include <getopt.h>

#include <event2/event.h>
#include <event2/util.h>

#include "eserror.h"
#include "log.h"
#include "esosip.h"
#include "escli.h"

/**
 * @brief
 */
typedef struct app_s {
   struct event_config  *cfg;            //!< LibEvent configuration
   struct event_base    *base;           //!< LibEvent Base loop
   struct event         *evsig;          //!< LibEvent Signal
   es_log_t             *logger;         //!< Logger handler
   es_osip_t            *osipCtx;        //!< OSip stack context
   es_cli_t             *cliCtx;         //!< Telnet Interface
} app_t;

#define ESIP_SHORT_OPT_VERSION_CHAR    "v"
#define ESIP_SHORT_OPT_HELP_CHAR       "h"

#define ESIP_SHORT_OPTS_STR \
   ESIP_SHORT_OPT_VERSION_CHAR \
   ESIP_SHORT_OPT_HELP_CHAR

#define ESIP_USAGE_MSG_TEXT_STR \
   "Usage: " PACKAGE " [OPTs]\n" \
   "\n" \
   "Operating modes:\n\n" \
   "  --" \
         ESIP_SHORT_OPT_HELP_CHAR \
         "\tPrint this message and then exit." \
         "\n" \
   "  --" \
         ESIP_SHORT_OPT_VERSION_CHAR \
         "\tPrint version information and then exit." \
         "\n" \
   "\n" \
   "For more information, contact me " PACKAGE_BUGREPORT "\n" \
   "\n"

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

static void esip_usage(void)
{
   fprintf(stdout, ESIP_USAGE_MSG_TEXT_STR);
}

static es_status esip_getopt(app_t *_pCtx, const int argc, char *argv[])
{
   es_status ret = ES_OK;
   if (_pCtx == (app_t *)0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Application context is null");
      return ES_ERROR_BADPARAM;
   }

   {
      int optChar = -1;
      do {
         optChar = getopt(argc, argv, ESIP_SHORT_OPTS_STR);
         if (optChar > 0) {
            ret = ES_OK;
         }
         switch (optChar) {
         case 'h':
            esip_usage();
            exit(EXIT_SUCCESS);
         case 'v':
#if defined(PACKAGE_NAME) && defined(PACKAGE_VERSION)
            fprintf(stdout, "%s version %s\n", PACKAGE_NAME, PACKAGE_VERSION);
#else
            fprintf(stdout, "Easy SIP version 0.0.0\n");
#endif
            fprintf(stdout, "Copyright (C) 2014 %s\n", PACKAGE_BUGREPORT);
            exit(EXIT_SUCCESS);
            break;
         case -1:
            break;
         default:
            esip_usage();
            return ES_ERROR_UNINITIALIZED;
         }
      } while(optChar > 0);
   }

   return ret;
}


int main(const int argc, char *argv[])
{
   app_t ctx;
   int ret = EXIT_SUCCESS;

   if (esip_getopt(&ctx, argc, argv) != ES_OK) {
      ESIP_TRACE(ESIP_LOG_CRIT, "Bad configuration");
      return EXIT_FAILURE;
   }

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

   /* Init CLI */
   if (es_cli_init(&ctx.cliCtx, ctx.base) != ES_OK) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Can not initialize CLI");
      goto ERROR_EXIT;
   }

   if (es_cli_start(ctx.cliCtx) != ES_OK) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Can not Start CLI");
      goto ERROR_EXIT;
   }

   if (es_osip_start(ctx.osipCtx) != ES_OK) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Can not Start OSip stack");
      goto ERROR_EXIT;
   }

   ESIP_TRACE(ESIP_LOG_DEBUG, "Starting %s v%s main loop", PACKAGE, VERSION);
   if (event_base_dispatch(ctx.base) != 0) {
      ESIP_TRACE(ESIP_LOG_EMERG, "Can not start main event lopp");
      goto ERROR_EXIT;
   }

   es_osip_stop(ctx.osipCtx);
   es_cli_stop(ctx.cliCtx);

   es_osip_deinit(ctx.osipCtx);
   es_cli_deinit(ctx.cliCtx);

   goto EXIT;

ERROR_EXIT:
   ESIP_TRACE(ESIP_LOG_EMERG, "%s fatal error: stoped.", PACKAGE);
   ret = EXIT_FAILURE;

EXIT:
  /* free cfg */
  event_config_free(ctx.cfg);
  ctx.cfg = NULL;

  event_free(ctx.evsig);
  event_base_free(ctx.base);

  return ret;
}

// vim: noai:ts=2:sw=2
