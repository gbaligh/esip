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

#ifndef _ESIP_LOG_H_
#define _ESIP_LOG_H_

/** LOGGING LEVELS */
#define ESIP_LOG_EMERG    0
#define ESIP_LOG_ALERT    1
#define ESIP_LOG_CRIT     2
#define ESIP_LOG_ERROR    3
#define ESIP_LOG_WARNING  4
#define ESIP_LOG_NOTICE   5
#define ESIP_LOG_INFO     6
#define ESIP_LOG_DEBUG    7

#define ES_EMERG(msg, ...) \
  do { \
    printf("[%s][EMERGENCY]- " msg "\r\n", PACKAGE, ##__VA_ARGS__); \
  } while(0)

#define ES_ALERT(msg, ...) \
  do { \
    printf("[%s][ALERT]- " msg "\r\n", PACKAGE, ##__VA_ARGS__); \
  } while(0)

/** */
#define ESIP_TRACE(level, msg, ...) \
  es_log_trace(NULL, level, msg, ##__VA_ARGS__)

typedef struct es_log_s es_log_t;

typedef void (es_log_print_cb)(const int level, const char * p, const size_t len, void * arg);

extern es_log_t es_log_default_handler;

es_status es_log_init(es_log_t ** handler);
es_status es_log_destroy(es_log_t * handler);
es_status es_log_set_loglevel(es_log_t * handler, unsigned int level);
es_status es_log_get_loglevel(es_log_t * handler, unsigned int * level);
es_status es_log_set_print_cb(es_log_t * handler, es_log_print_cb cb, void * arg);

void es_log_trace(es_log_t * handler, unsigned int level, const char const * format, ...);
void es_log_emerg(es_log_t * handler, const char const * format, ...);
void es_log_alert(es_log_t * handler, const char const * format, ...);
void es_log_crit(es_log_t * handler, const char const * format, ...);
void es_log_error(es_log_t * handler, const char const * format, ...);
void es_log_warning(es_log_t * handler, const char const * format, ...);
void es_log_notice(es_log_t * handler, const char const * format, ...);
void es_log_info(es_log_t * handler, const char const * format, ...);
void es_log_debug(es_log_t * handler, const char const * format, ...);

#endif /* _ESIP_LOG_H_ */
