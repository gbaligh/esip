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

#ifndef _ES_TRANSPORT_H_
#define _ES_TRANSPORT_H_

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct es_transport_s es_transport_t;

typedef void (*es_transport_event_cb)(
      IN es_transport_t  * transp,
      IN int                  ev,
      IN int                  error,
      OUT void    *    ctx);

typedef void (*es_transport_msg_recv_cb)(
      IN es_transport_t   *   transp,
      IN const char const  * msg,
      IN const unsigned int   size,
      OUT void    *    ctx);


struct es_transport_callbacks_s {
   es_transport_event_cb event_cb;
   es_transport_msg_recv_cb msg_recv_cb;
   void * user_data;
};

es_status es_transport_init(es_transport_t **ppCtx, struct event_base * pBase);

es_status es_transport_set_callbacks(es_transport_t *pCtx, struct es_transport_callbacks_s * cs);

es_status es_transport_start(es_transport_t *pCtx);

es_status es_transport_stop(es_transport_t *pCtx);

es_status es_transport_destroy(es_transport_t *pCtx);

es_status es_transport_get_udp_socket(es_transport_t *pCtx, int * fd);

es_status es_transport_send(es_transport_t *pCtx, char * ip, int port, const char * msg, size_t size);

#if defined(__cplusplus)
}
#endif /* __cplusplus */
#endif /* _ES_TRANSPORT_H_ */
