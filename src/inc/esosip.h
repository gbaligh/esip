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

#ifndef _ES_OSIP_H_
#define _ES_OSIP_H_

/** @brief */
typedef struct es_osip_s es_osip_t;

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * @brief es_osip_init
 * @param ctx
 * @param base
 * @return
 */
es_status es_osip_init(es_osip_t ** ctx, struct event_base * base);

/**
 * @brief es_osip_start
 * @param _ctx
 * @return
 */
es_status es_osip_start(es_osip_t *pCtx);

/**
 * @brief es_osip_stop
 * @param pCtx
 * @return
 */
es_status es_osip_stop(es_osip_t *pCtx);

/**
 * @brief es_osip_deinit
 * @param pCtx
 * @return
 */
es_status es_osip_deinit(es_osip_t *pCtx);

/**
 * @brief es_osip_parse_msg
 * @param ctx
 * @param buf
 * @param size
 * @return
 */
es_status es_osip_parse_msg(es_osip_t * ctx, const char * buf, unsigned int size);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif
