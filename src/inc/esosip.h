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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct es_osip_s es_osip_t;

es_status es_osip_init(OUT es_osip_t **ctx, struct event_base *base);

es_status es_osip_parse_msg(IN es_osip_t *ctx, const char *buf, unsigned int size);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif
