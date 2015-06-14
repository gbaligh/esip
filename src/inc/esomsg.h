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

#ifndef ESOMSG_H
#define ESOMSG_H

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct es_msg_s es_msg_t;

es_status es_msg_initRequest(es_msg_t **ppCtx);

es_status es_msg_initResponse(osip_message_t      **ppRes,
                              int                 respCode,
                              osip_message_t      *req);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif
