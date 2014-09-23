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

#include <osip2/osip.h>

#include "eserror.h"
#include "log.h"
#include "esomsg.h"

#define ES_OMSG_MAGIC      0X20140917

struct es_msg_s {
   /* Magic */
   uint32_t                  magic;
   /* MSG Ctx */
   osip_message_t            *omsg;
};

es_status es_msg_initRequest(es_msg_t **ppCtx)
{
   struct es_msg_s *_pCtx = (struct es_msg_s *) malloc(sizeof(struct es_msg_s));
   if (_pCtx == (struct es_msg_s *)0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Memory error");
      return ES_ERROR_OUTOFRESOURCES;
   }

   _pCtx->magic = ES_OMSG_MAGIC;

   {
      osip_message_t *_pMsg = NULL;

      /* Create an emty message */
      if (osip_message_init(&_pMsg) != OSIP_SUCCESS) {
         ESIP_TRACE(ESIP_LOG_ERROR, "Memory error");
         free(_pCtx);
         return ES_ERROR_OUTOFRESOURCES;
      }

      /* Set SIP Version */
      osip_message_set_version(_pMsg, osip_strdup("SIP/2.0"));

      /* Init VIA header */
      {
         osip_via_t *viaHdr = NULL;
         if (osip_via_init(&viaHdr) != OSIP_SUCCESS) {
            ESIP_TRACE(ESIP_LOG_ERROR, "Memory error");
            osip_message_free(_pMsg);
            free(_pCtx);
            return ES_ERROR_OUTOFRESOURCES;
         }

         if (osip_list_add(&_pMsg->vias, viaHdr, -1) != OSIP_SUCCESS) {
            ESIP_TRACE(ESIP_LOG_ERROR, "Memory error");
            osip_message_free(_pMsg);
            free(_pCtx);
            return ES_ERROR_OUTOFRESOURCES;
         }
      }

      /* Init From header */
      if (osip_from_init(&_pMsg->from) != OSIP_SUCCESS) {
         ESIP_TRACE(ESIP_LOG_ERROR, "Memory error");
         osip_message_free(_pMsg);
         free(_pCtx);
         return ES_ERROR_OUTOFRESOURCES;
      }

      /* Init To header */
      if (osip_to_init(&_pMsg->to) != OSIP_SUCCESS) {
         ESIP_TRACE(ESIP_LOG_ERROR, "Memory error");
         osip_message_free(_pMsg);
         free(_pCtx);
         return ES_ERROR_OUTOFRESOURCES;
      }

      /* Set CSeq header */
      if (osip_cseq_init(&_pMsg->cseq) != OSIP_SUCCESS) {
         ESIP_TRACE(ESIP_LOG_ERROR, "Memory error");
         osip_message_free(_pMsg);
         free(_pCtx);
         return ES_ERROR_OUTOFRESOURCES;
      }

      /* Set Call-Id header */
      if (osip_call_id_init(&_pMsg->call_id) != OSIP_SUCCESS) {
         ESIP_TRACE(ESIP_LOG_ERROR, "Memory error");
         osip_message_free(_pMsg);
         free(_pCtx);
         return ES_ERROR_OUTOFRESOURCES;
      }

      /* Set User-Agent header */
      if (osip_message_set_user_agent(_pMsg, osip_strdup(PACKAGE_STRING)) != OSIP_SUCCESS) {
         ESIP_TRACE(ESIP_LOG_ERROR, "Memory error");
         osip_message_free(_pMsg);
         free(_pCtx);
         return ES_ERROR_OUTOFRESOURCES;
      }

      _pCtx->omsg = _pMsg;
   }

   *ppCtx = _pCtx;
   return ES_OK;
}

es_status es_msg_initResponse(osip_message_t      **ppRes,
                              int                 respCode,
                              osip_message_t      *req)
{
   osip_message_t *_pMsg = NULL;

   {
      /* Check validity */
      {
         if (req->to == NULL) {
            ESIP_TRACE(ESIP_LOG_ERROR, "empty To in request header");
            return ES_ERROR_NULLPTR;
         }
         if (req->from == NULL) {
            ESIP_TRACE(ESIP_LOG_ERROR, "empty From in request header");
            return ES_ERROR_NULLPTR;
         }
      }

      /* Create an emty message */
      if (osip_message_init(&_pMsg) != OSIP_SUCCESS) {
         ESIP_TRACE(ESIP_LOG_ERROR, "Memory error");
         return ES_ERROR_OUTOFRESOURCES;
      }

      /* Set SIP Version */
      osip_message_set_version(_pMsg, osip_strdup("SIP/2.0"));

      osip_message_set_max_forwards(_pMsg, "70");

      /* Set status code */
      if (respCode > 0) {
         osip_message_set_status_code(_pMsg, respCode);
         osip_message_set_reason_phrase(_pMsg, osip_strdup(osip_message_get_reason(respCode)));
      }

      /* Set From header */
      osip_from_clone(req->from, &_pMsg->from);

      /* Set To header */
      osip_to_clone(req->to, &_pMsg->to);

      {
         osip_uri_param_t *tag = NULL;
         unsigned int random_tag = 0;
         char str_random[256];
         osip_to_get_tag(_pMsg->to, &tag);
         if (tag == NULL) {
            random_tag = osip_build_random_number();
            snprintf(str_random, sizeof(str_random), "%d", random_tag);
            osip_to_set_tag(_pMsg->to, osip_strdup(str_random));
         }
      }

      /* Set CSeq header */
      if (osip_cseq_clone(req->cseq, &_pMsg->cseq) != OSIP_SUCCESS) {
         ESIP_TRACE(ESIP_LOG_ERROR, "Memory error");
         osip_message_free(_pMsg);
         return ES_ERROR_OUTOFRESOURCES;
      }

      /* Set Call-Id header */
      if (osip_call_id_clone(req->call_id, &_pMsg->call_id) != OSIP_SUCCESS) {
         ESIP_TRACE(ESIP_LOG_ERROR, "Memory error");
         osip_message_free(_pMsg);
         return ES_ERROR_OUTOFRESOURCES;
      }

      /* Handle Via header */
      {
         int pos = 0;
         while (!osip_list_eol(&req->vias, pos)) {
            osip_via_t * via = NULL;
            osip_via_t * via2 = NULL;

            via = (osip_via_t *) osip_list_get(&req->vias, pos);
            if (osip_via_clone(via, &via2) != OSIP_SUCCESS) {
               ESIP_TRACE(ESIP_LOG_ERROR, "Memory error");
               osip_message_free(_pMsg);
               return ES_ERROR_OUTOFRESOURCES;
            }
            osip_list_add(&(_pMsg->vias), via2, -1);
            pos++;
         }
      }

      /* Set User-Agent header */
      osip_message_set_user_agent(_pMsg, PACKAGE);
      osip_message_set_organization(_pMsg, PACKAGE_STRING);
      osip_message_set_subject(_pMsg, "Testing with " PACKAGE_STRING);
      osip_message_set_server(_pMsg, PACKAGE_STRING " Server");
   }

   *ppRes = _pMsg;
   return ES_OK;
}
