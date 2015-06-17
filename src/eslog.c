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

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>

#include <event2/event.h>
#include <event2/util.h>

#include "eserror.h"
#include "log.h"

#define ES_LOG_COLOR_RED_BEGIN    "\x1b[31m"

#define ES_LOG_COLOR_YELLOW_BEGIN "\x1b[33m"

#define ES_LOG_COLOR_GREEN_BEGIN  "\x1b[32m"

#define ES_LOG_COLOR_END          "\x1b[;m"

#ifdef PACKAGE_NAME
#define ES_LOG_DEFAULT_MODULE     PACKAGE_NAME
#else
#define ES_LOG_DEFAULT_MODULE     "easylog"
#endif

struct es_log_s {
   const char *module;
   uint32_t loglevel;
   uint32_t flags;
   FILE *f;

   es_log_print_cb *cb;
   void * cb_arg;
};

static const char const *log_level_str[] = {
   "EMERGENCY",
   "ALERT",
   "CRITIC",
   "ERROR",
   "WARNING",
   "NOTICE",
   "INFO",
   "DEBUG"
};

es_log_t es_log_default_handler = {
   ES_LOG_DEFAULT_MODULE,
   ESIP_LOG_DEBUG,
   0,
   NULL,
   NULL,
   NULL
};

es_status es_log_init(es_log_t **handler)
{
   struct es_log_s * h = (struct es_log_s *) malloc(sizeof(struct es_log_s));
   if (h == NULL) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Can not initialize log: no more memory");
      return ES_ERROR_OUTOFRESOURCES;
   }

   memset(h, 0x00, sizeof(struct es_log_s));

   h->module = ES_LOG_DEFAULT_MODULE;
   h->loglevel = ESIP_LOG_DEBUG;

   *handler = h;
   return ES_OK;
}

es_status es_log_destroy(es_log_t *handler)
{
   if (handler == NULL) {
      ESIP_TRACE(ESIP_LOG_ERROR, "bad arguments");
      return ES_ERROR_NULLPTR;
   }

   free(handler);
   return ES_OK;
}

es_status es_log_set_loglevel(es_log_t *handler, unsigned int level)
{
   if (handler == NULL) {
      ESIP_TRACE(ESIP_LOG_ERROR, "bad arguments");
      return ES_ERROR_NULLPTR;
   }

   handler->loglevel = level;
   return ES_OK;
}

es_status es_log_get_loglevel(es_log_t *handler, unsigned int *level)
{
   if (handler == NULL) {
      ESIP_TRACE(ESIP_LOG_ERROR, "bad arguments");
      return ES_ERROR_NULLPTR;
   }

   *level = handler->loglevel;
   return ES_OK;
}

es_status es_log_set_print_cb(es_log_t *handler, es_log_print_cb cb, void *arg)
{
   if (handler == NULL) {
      ESIP_TRACE(ESIP_LOG_ERROR, "bad arguments");
      return ES_ERROR_NULLPTR;
   }

   handler->cb = cb;
   handler->cb_arg = arg;

   return ES_OK;
}

static void es_log_vprintf(es_log_t *handler, const char *function, const int line, const unsigned int level, const char const *fmt, va_list ap)
{
   if (handler == NULL) {
      ESIP_TRACE(ESIP_LOG_ERROR, "bad arguments");
      return;
   }

   if (level > handler->loglevel) {
      return;
   }

   switch (level) {
   case ESIP_LOG_EMERG:
   case ESIP_LOG_ALERT:
   case ESIP_LOG_CRIT:
   case ESIP_LOG_ERROR:
      fprintf(stderr, "%s", ES_LOG_COLOR_RED_BEGIN);
      break;
   case ESIP_LOG_WARNING:
      fprintf(stderr, "%s", ES_LOG_COLOR_YELLOW_BEGIN);
      break;
   case ESIP_LOG_NOTICE:
      fprintf(stderr, "%s", ES_LOG_COLOR_GREEN_BEGIN);
      break;
   case ESIP_LOG_INFO:
   case ESIP_LOG_DEBUG:
   default:
      fprintf(stderr, "%s", ES_LOG_COLOR_END);
      break;
   }

   fprintf(stderr, "[%s][%s]\t", handler->module, log_level_str[level]);

   if (level == ESIP_LOG_DEBUG) {
      fprintf(stderr, "%s(%d): ", function, line);
   }

   (void)vfprintf(stderr, fmt, ap);

   fprintf(stderr, "%s\r\n", ES_LOG_COLOR_END);
   fflush(stderr);
}

void es_log_trace(es_log_t * handler, const char *func, const int line, unsigned int level, const char const *format, ...)
{
   va_list ap;

   if (handler == NULL) {
      handler = &es_log_default_handler;
   }

   va_start(ap, format);
   es_log_vprintf(handler, func, line, level, format, ap);
   va_end(ap);
}

void es_log_nop(es_log_t *handler, const char const *format, ...)
{
  ESIP_TRACE(ESIP_LOG_ERROR, "Not implemented");
}

void es_log_emerg(es_log_t *handler, const char const *format, ...)
{
  ESIP_TRACE(ESIP_LOG_ERROR, "Not implemented");
}

void es_log_alert(es_log_t *handler, const char const *format, ...)
{
  ESIP_TRACE(ESIP_LOG_ERROR, "Not implemented");
}

void es_log_crit(es_log_t *handler, const char const *format, ...)
{
  ESIP_TRACE(ESIP_LOG_ERROR, "Not implemented");
}

void es_log_error(es_log_t *handler, const char const *format, ...)
{
  ESIP_TRACE(ESIP_LOG_ERROR, "Not implemented");
}

void es_log_warning(es_log_t *handler, const char const *format, ...)
{
  ESIP_TRACE(ESIP_LOG_ERROR, "Not implemented");
}

void es_log_notice(es_log_t *handler, const char const *format, ...)
{
  ESIP_TRACE(ESIP_LOG_ERROR, "Not implemented");
}

void es_log_info(es_log_t *handler, const char const *format, ...)
{
  ESIP_TRACE(ESIP_LOG_ERROR, "Not implemented");
}

void es_log_debug(es_log_t *handler, const char const *format, ...)
{
  ESIP_TRACE(ESIP_LOG_ERROR, "Not implemented");
}
