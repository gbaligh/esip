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
#include <osip2/osip_dialog.h>

#include "eserror.h"
#include "log.h"

#include "estransport.h"
#include "esomsg.h"
#include "esosip.h"

#define ES_OSIP_MAGIC       0x20140607

struct es_osip_s {
   /* Magic */
   uint32_t                  magic;
   /* OSip */
   osip_t                    *osip;
   /* Transport Layer */
   es_transport_t            *transportCtx;
   /* base loop thread */
   struct event_base         *base;
   /* Pending Event to handle */
   osip_list_t               pendingEv;
   /* Dialog list */
   osip_list_t               osipDialog;
};

/*******************************************************************************
                        Internal static functions
 ******************************************************************************/

/**
 * @brief _es_transport_event_cb
 * Transport layer event callback
 *
 * @param transp
 * @param event
 * @param error
 * @param data
 */
static void _es_transport_event_cb(es_transport_t *transp, int event, int error, void *data);

/**
 * @brief _es_transport_msg_cb
 * Transport layer new msg callback
 *
 * @param transp
 * @param msg
 * @param size
 * @param data
 */
static void _es_transport_msg_cb(es_transport_t *transp, const char const *msg, const unsigned int size, void *data);

/**
 * @brief Set OSip stack callbacks to internal ones
 * @return ES_OK on success
 */
static es_status _es_osip_set_internal_callbacks(struct es_osip_s *_ctx);

/**
 * @brief ESip Event sender
 * Using LibEvent, this function create a nex event
 * and activate it to unlock the base loop
 *
 * @return ES_OK on success
 */
static es_status _es_osip_wakeup(struct es_osip_s *_ctx);

/*******************************************************************************
                        Public functions implementation
 ******************************************************************************/

es_status es_osip_init(es_osip_t **pCtx, struct event_base *base)
{
   es_status ret = ES_OK;

   struct es_osip_s * _pCtx = (struct es_osip_s *) malloc(sizeof(struct es_osip_s));
   if (_pCtx == NULL) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Can not initialize OSip stack: no more memory");
      return ES_ERROR_OUTOFRESOURCES;
   }

   memset(_pCtx, 0, sizeof(struct es_osip_s));

   /* Set Magic */
   _pCtx->magic = ES_OSIP_MAGIC;

   /* Init Transport Layer */
   ret = es_transport_init(&_pCtx->transportCtx, base);
   if (ret != ES_OK) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Initializing Transport Layer failed");
      free(_pCtx);
      return ret;
   }

   /* Init Transport Layer Callbacks */
   {
      struct es_transport_callbacks_s cs = {
         &_es_transport_event_cb,
         &_es_transport_msg_cb,
         _pCtx
      };

      ret = es_transport_set_callbacks(_pCtx->transportCtx, &cs);
      if (ret != ES_OK) {
         ESIP_TRACE(ESIP_LOG_ERROR, "Setting Callbacks for Transport Layer failed");
         free(_pCtx);
         return ret;
      }
   }

   /* Init OSip */
   if (osip_init(&_pCtx->osip) != OSIP_SUCCESS) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Can not initialize OSip stack");
      free(_pCtx);
      return ES_ERROR_UNKNOWN;
   }

   /* list of pending event */
   if (osip_list_init(&_pCtx->pendingEv) != OSIP_SUCCESS) {
      ESIP_TRACE(ESIP_LOG_ERROR, "List for pending event initialization failed");
      free(_pCtx->osip);
      free(_pCtx);
      return ES_ERROR_OUTOFRESOURCES;
   }

   /* list of dialog */
   if (osip_list_init(&_pCtx->osipDialog) != OSIP_SUCCESS) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Dialog list failed");
      free(_pCtx->osip);
      free(_pCtx);
      return ES_ERROR_OUTOFRESOURCES;
   }

   /* Set internal OSip Callback */
   if ((ret = _es_osip_set_internal_callbacks(_pCtx)) != ES_OK) {
      free(_pCtx->osip);
      free(_pCtx);
      return ret;
   }

   /* Set base event thread to use */
   _pCtx->base = base;

   /* Ok */
   *pCtx = _pCtx;
   return ES_OK;
}

es_status es_osip_start(es_osip_t *pCtx)
{
   es_status ret = ES_OK;

   struct es_osip_s *_pCtx = (struct es_osip_s *)pCtx;
   if (_pCtx == NULL) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Can not initialize OSip stack: no more memory");
      return ES_ERROR_OUTOFRESOURCES;
   }

   ret = es_transport_start(_pCtx->transportCtx);
   if (ret != ES_OK) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Start Transport Layer failed");
      return ret;
   }

   return ret;
}

es_status es_osip_stop(es_osip_t *pCtx)
{
   es_status ret = ES_OK;

   struct es_osip_s *_pCtx = (struct es_osip_s *)pCtx;
   if (_pCtx == (struct es_osip_s *)0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Can not initialize OSip stack: no more memory");
      return ES_ERROR_NULLPTR;
   }

   if (_pCtx->magic != ES_OSIP_MAGIC) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Bad Magic %d - ptr(%p)(%d)", ES_OSIP_MAGIC, _pCtx, _pCtx->magic);
      return ES_ERROR_NULLPTR;
   }

   ret = es_transport_stop(_pCtx->transportCtx);
   if (ret != ES_OK) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Start Transport Layer failed");
      return ret;
   }

   return ret;
}

static void _es_osip_list_freeEl(void *pEl)
{
   struct event *_pEvSip = (struct event *)pEl;
   if (_pEvSip == (struct event *)0) {
      return;
   }

   event_free(_pEvSip);
}

es_status es_osip_deinit(es_osip_t *pCtx)
{
   struct es_osip_s *_pCtx = (struct es_osip_s *)pCtx;
   if (_pCtx == (struct es_osip_s *)0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Bad Context pointer ptr(%p)", _pCtx);
      return ES_ERROR_NULLPTR;
   }

   if (_pCtx->magic != ES_OSIP_MAGIC) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Bad Magic %d - ptr(%p)(%d)", ES_OSIP_MAGIC, _pCtx, _pCtx->magic);
      return ES_ERROR_NULLPTR;
   }

   osip_list_special_free(&_pCtx->pendingEv, _es_osip_list_freeEl);

   es_transport_destroy(_pCtx->transportCtx);
   
   osip_release(_pCtx->osip);
   
   free(_pCtx);

   return ES_OK;
}

es_status es_osip_parse_msg(es_osip_t *pCtx, const char *buf, unsigned int size)
{
   osip_event_t * evt = (osip_event_t *)0;
   struct es_osip_s *_pCtx = (struct es_osip_s *)pCtx;
   osip_transaction_t *tr = (osip_transaction_t *)0;

   ESIP_TRACE(ESIP_LOG_DEBUG, "Enter");

   /* Check Context */
   if (_pCtx == (struct es_osip_s *)0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "SIP ctx is null");
      return ES_ERROR_NULLPTR;
   }

   /* Check Context magic */
   if (_pCtx->magic != ES_OSIP_MAGIC) {
      ESIP_TRACE(ESIP_LOG_ERROR, "SIP ctx is not valid");
      return ES_ERROR_INVALID_HANDLE;
   }

   /* Parse buffer and check if it's really a SIP Message */
   evt = osip_parse(buf, size);
   if (evt == (osip_event_t *)0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Error creating OSip event");
      return ES_ERROR_NETWORK_PROBLEM;
   }

   ESIP_TRACE(ESIP_LOG_INFO,"received SIP type %s: %s",
              (MSG_IS_REQUEST(evt->sip))? "REQUEST" : "RESPONSE",
              (MSG_IS_REQUEST(evt->sip) ? ((evt->sip->sip_method)    ? evt->sip->sip_method    : "NULL") :
                                          ((evt->sip->reason_phrase) ? evt->sip->reason_phrase : "NULL")));

   if (EVT_IS_RCV_STATUS_1XX(evt)) {
      tr = osip_transaction_find(&_pCtx->osip->osip_ist_transactions, evt);
      if (tr == (osip_transaction_t *)0) {
         ESIP_TRACE(ESIP_LOG_INFO, "No transaction for MESSAGE event");
         free(evt);
         return ES_ERROR_ILLEGAL_ACTION;
      }
   }

   if( EVT_IS_RCV_STATUS_2XX(evt) || EVT_IS_RCV_STATUS_3456XX(evt)) {
      tr = osip_transaction_find(&_pCtx->osip->osip_ist_transactions, evt);
      if (tr == (osip_transaction_t *)0) {
         ESIP_TRACE(ESIP_LOG_INFO, "No transaction for MESSAGE event");
         free(evt);
         return ES_ERROR_ILLEGAL_ACTION;
      }
   }

   if (EVT_IS_RCV_ACK(evt)) {
      int _i = 0;
      osip_dialog_t *dialog = (osip_dialog_t *)0;
      for (_i=0; !osip_list_eol(&_pCtx->osipDialog, _i); ++_i, dialog=(osip_dialog_t *)0) {
         dialog = (osip_dialog_t *)osip_list_get(&_pCtx->osipDialog, _i);
         if (osip_dialog_match_as_uas(dialog, evt->sip) == OSIP_SUCCESS) {
            ESIP_TRACE(ESIP_LOG_DEBUG, "Dialog for ACK found [STATE:%d]", dialog->state);
            break;
         }
      }

      if (dialog != (osip_dialog_t *)0); {
         osip_stop_retransmissions_from_dialog(_pCtx->osip, dialog);
      }

      free(evt);
      return ES_OK;
   }

   if (EVT_IS_RCV_INVITE(evt)) {
      ESIP_TRACE(ESIP_LOG_INFO, "New INVITE transaction");
      /* Init a new INVITE Server Transaction */
      if (osip_transaction_init(&tr, IST, _pCtx->osip, evt->sip) != OSIP_SUCCESS) {
         ESIP_TRACE(ESIP_LOG_ERROR, "Erro init new transation");
         free(evt);
         return ES_ERROR_OUTOFRESOURCES;
      }
   }

   if (EVT_IS_RCV_REQUEST(evt)) {
      ESIP_TRACE(ESIP_LOG_INFO, "New NON INVITE transaction");
      /* Init a new NON INVITE Server Transaction */
      if (osip_transaction_init(&tr, NIST, _pCtx->osip, evt->sip) != OSIP_SUCCESS) {
         ESIP_TRACE(ESIP_LOG_ERROR, "Erro init new transation");
         free(evt);
         return ES_ERROR_OUTOFRESOURCES;
      }
   }

   if (tr != (osip_transaction_t *)0) {
      /* Set Out Socket for the new Transaction, used to send response */
      {
         int fd = -1;
         es_transport_get_udp_socket(_pCtx->transportCtx, &fd);
         if (osip_transaction_set_out_socket(tr, fd) != OSIP_SUCCESS) {
            ESIP_TRACE(ESIP_LOG_ERROR, "Setting socket descriptor failed");
            osip_transaction_free(tr);
            free(evt);
            return ES_ERROR_UNKNOWN;
         }
      }

      /* Set context reference into Transaction struct */
      osip_transaction_set_your_instance(tr, (void *)_pCtx);

      /* add a new OSip event into FiFo list */
      if (osip_transaction_add_event(tr, evt)) {
         ESIP_TRACE(ESIP_LOG_ERROR, "adding event failed");
         osip_transaction_free(tr);
         free(evt);
         return ES_ERROR_OUTOFRESOURCES;
      }

      /* Send notification using event for the transaction */
      if (_es_osip_wakeup(_pCtx) != ES_OK) {
         ESIP_TRACE(ESIP_LOG_ERROR, "sending event failed");
         free(evt);
         return ES_ERROR_UNKNOWN;
      }
   } else {
      free(evt);
   }

   /* Look for existant Dialog */

   return ES_OK;
}

/*******************************************************************************
                  Internal static functions implementation
 ******************************************************************************/

static void _es_transport_event_cb(es_transport_t *transp, int event, int error, void *data)
{
   ESIP_TRACE(ESIP_LOG_DEBUG, "Event: %d", event);
}

static void _es_transport_msg_cb(es_transport_t *transp, const char const *msg, const unsigned int size, void *data)
{
   struct es_osip_s * ctx = (struct es_osip_s *)data;

   if (ctx == (struct es_osip_s *)0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Context reference is invalid");
      return;
   }

   if (size < 3) {
      ESIP_TRACE(ESIP_LOG_WARNING, "Message length [%d] can not be right !", size);
   }

   ESIP_TRACE(ESIP_LOG_DEBUG, "Received:\n<=====\n%s\n<=====", msg);

   if (es_osip_parse_msg(ctx, msg, size) != ES_OK) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Error parsing Message!");
      return;
   }
}

static void _es_osip_loop(evutil_socket_t fd, short event, void *arg)
{
   struct es_osip_s * _pCtx = (struct es_osip_s *)arg;
   /* Check Context */
   if (_pCtx == (struct es_osip_s *)0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "SIP ctx is null");
      return;
   }

   /* Loop until we have no pending event,
   * blocking: if there is too mush events
   * and we are using the same main loop thread !! */
   while ( (osip_list_size(&_pCtx->pendingEv) > 0) ) {

      /* Get the first event from the list */
      struct event * ev = (struct event *)osip_list_get(&_pCtx->pendingEv, 0);

      ESIP_TRACE(ESIP_LOG_DEBUG, "pending event %p", ev);

      /* Remove this event since it handled now */
      osip_list_remove(&_pCtx->pendingEv, 0);

      /* INVITE Client Transaction Fifo list */
      ESIP_TRACE(ESIP_LOG_DEBUG, "Check pending ICT event...");
      if (osip_ict_execute(_pCtx->osip) != OSIP_SUCCESS) {
         ESIP_TRACE(ESIP_LOG_ERROR,"== ICT failed");
      }

      /* INVITE Server Transaction Fifo list */
      ESIP_TRACE(ESIP_LOG_DEBUG, "Check pending IST event...");
      if (osip_ist_execute(_pCtx->osip) != OSIP_SUCCESS) {
         ESIP_TRACE(ESIP_LOG_ERROR,"== IST failed");
      }

      /* Non-INVITE Client Transaction Fifo list */
      ESIP_TRACE(ESIP_LOG_DEBUG, "Check pending NICT event...");
      if (osip_nict_execute(_pCtx->osip) != OSIP_SUCCESS) {
         ESIP_TRACE(ESIP_LOG_ERROR,"== NICT failed");
      }

      /* Non-INVITE Server Transaction Fifo list */
      ESIP_TRACE(ESIP_LOG_DEBUG, "Check pending NIST event...");
      if (osip_nist_execute(_pCtx->osip) != OSIP_SUCCESS) {
         ESIP_TRACE(ESIP_LOG_ERROR,"== NIST failed");
      }

      /* TODO: For timer OSIP event we must find an other method to wake up libEvent */

      /* INVITE Client Transation Timer Event Fifo list */
      ESIP_TRACE(ESIP_LOG_DEBUG, "Check pending TIMER-ICT event...");
      osip_timers_ict_execute(_pCtx->osip);

      /* INVITE Server Transaction Timer Event Fifo list */
      ESIP_TRACE(ESIP_LOG_DEBUG, "Check pending TIMER-IST event...");
      osip_timers_ist_execute(_pCtx->osip);

      /* Non-INVITE Client Transaction Timer Event Fifo list */
      ESIP_TRACE(ESIP_LOG_DEBUG, "Check pending TIMER-NICT event...");
      osip_timers_nict_execute(_pCtx->osip);

      /* Non-INVITE Server Transaction Timer Event Fifo list */
      ESIP_TRACE(ESIP_LOG_DEBUG, "Check pending TIMER-NIST event...");
      osip_timers_nist_execute(_pCtx->osip);

      event_free(ev);
   }
}

static es_status _es_osip_wakeup(struct es_osip_s *pCtx)
{
   struct event * _pEvSip = (struct event *)0;

   ESIP_TRACE(ESIP_LOG_DEBUG, "Enter");

   /* Check Context */
   if (pCtx == (struct es_osip_s *)0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "SIP ctx is null");
      return ES_ERROR_NULLPTR;
   }

   /* Dispatch a new event to unblock the base loop thread */
   _pEvSip = event_new(pCtx->base, -1, (EV_READ), _es_osip_loop, (void *)pCtx);
   if (_pEvSip == (struct event *)0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Can not create event for OSip stack");
      return ES_ERROR_OUTOFRESOURCES;
   }

   /* set priority */
   if (event_priority_set(_pEvSip, 0) != 0) {
      ESIP_TRACE(ESIP_LOG_WARNING, "Can not set priority event for OSip stack");
   }

   /* Add the new event to base loop thread handler */
   if (event_add(_pEvSip, NULL) != 0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Can not make OSip event pending");
      event_del(_pEvSip);
      return ES_ERROR_OUTOFRESOURCES;
   }

   /* This is a pending event now, add it to Fifo list */
   osip_list_add(&pCtx->pendingEv, _pEvSip, 0);

   /* activate the event (callabck will be executed) */
   event_active(_pEvSip, EV_READ, 0);

   return ES_OK;
}

static int _es_internal_send_msg_cb(osip_transaction_t *tr, osip_message_t *msg, char *addr, int port, int socket)
{
   char * buf = NULL;
   size_t buf_len = 0;
   struct es_osip_s * _pCtx = (struct es_osip_s *)0;

   _pCtx = osip_transaction_get_your_instance(tr);
   if (_pCtx == (struct es_osip_s *)0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Reference is invalid");
      return -1;
   }

   /* Check magic */
   if (_pCtx->magic != ES_OSIP_MAGIC) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Reference is invalid");
      return -1;
   }

   osip_message_to_str(msg, &buf, &buf_len);
   
   ESIP_TRACE(ESIP_LOG_DEBUG,"Sending \n=====>\n%s\n=====>", buf);
   
   es_transport_send(_pCtx->transportCtx, addr, port, buf, buf_len);
   
   free(buf);
   return OSIP_SUCCESS;
}

static void _es_internal_transport_error_cb(int type, osip_transaction_t *tr, int error)
{
   ESIP_TRACE(ESIP_LOG_INFO,"Error Transport for Transaction %p type %d, error %d", tr, type, error);
}

static void _es_internal_kill_transaction_cb(int type, osip_transaction_t *tr)
{
   struct es_osip_s *_pCtx = NULL;
   ESIP_TRACE(ESIP_LOG_INFO,"Removing Transaction %p", tr);

   _pCtx = osip_transaction_get_your_instance(tr);
   if (_pCtx == (struct es_osip_s *)0) {
      return;
   }

   if (_pCtx->magic != ES_OSIP_MAGIC) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Reference is invalid");
      return;
   }
}

static void _es_internal_message_cb(int type, osip_transaction_t *tr, osip_message_t *msg)
{
   struct es_osip_s * _pCtx = (struct es_osip_s *)0;
   osip_message_t *_pResp = (osip_message_t *)0;
   osip_event_t * _pEvt = (osip_event_t *)0;
   int sendResp = 0;

   ESIP_TRACE(ESIP_LOG_DEBUG,"Enter: type %d", type);

   _pCtx = osip_transaction_get_your_instance(tr);
   if (_pCtx == (struct es_osip_s *)0) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Reference is invalid");
      return;
   }

   /* Check magic */
   if (_pCtx->magic != ES_OSIP_MAGIC) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Reference is invalid");
      return;
   }

   switch (type) {

   case OSIP_IST_INVITE_RECEIVED: {
      ESIP_TRACE(ESIP_LOG_INFO,"OSIP_IST_INVITE_RECEIVED");
      if (es_msg_initResponse(&_pResp, SIP_OK, msg) != ES_OK) {
         ESIP_TRACE(ESIP_LOG_ERROR, "Creating Response failed");
         return;
      }
      sendResp = 1;
   }
      break;

   case OSIP_IST_INVITE_RECEIVED_AGAIN: {
      ESIP_TRACE(ESIP_LOG_INFO,"OSIP_IST_INVITE_RECEIVED_AGAIN");
   }
      break;

   case OSIP_IST_STATUS_2XX_SENT: {
      ESIP_TRACE(ESIP_LOG_INFO,"OSIP_IST_STATUS_2XX_SENT");
      if (MSG_IS_RESPONSE_FOR(msg, "INVITE") && !MSG_TEST_CODE(msg, 100)) {
         osip_dialog_t *dialog = NULL;
         if (osip_dialog_init_as_uas(&dialog, tr->orig_request, msg) != OSIP_SUCCESS) {
            ESIP_TRACE(ESIP_LOG_ERROR, "Creating new dialog failed");
            return;
         }
         osip_dialog_set_instance(dialog, _pCtx);
         osip_dialog_update_route_set_as_uas(dialog, tr->orig_request);
         osip_list_add(&_pCtx->osipDialog, (void *)dialog, 0);
      }
   }
      break;

   case OSIP_NIST_STATUS_2XX_SENT: {
      ESIP_TRACE(ESIP_LOG_INFO,"OSIP_NIST_STATUS_2XX_SENT");
   }
      break;

   case OSIP_IST_ACK_RECEIVED: {
      ESIP_TRACE(ESIP_LOG_INFO,"OSIP_IST_ACK_RECEIVED");
   }
      break;

   case OSIP_IST_ACK_RECEIVED_AGAIN: {
      ESIP_TRACE(ESIP_LOG_INFO,"OSIP_IST_ACK_RECEIVED_AGAIN");
   }
      break;

   case OSIP_NIST_REGISTER_RECEIVED: {
      /* TODO: Send it to REGISTRAR module */
      ESIP_TRACE(ESIP_LOG_INFO,"OSIP_NIST_REGISTER_RECEIVED");
      if (es_msg_initResponse(&_pResp, SIP_OK, msg) != ES_OK) {
         ESIP_TRACE(ESIP_LOG_ERROR, "Creating Response failed");
         return;
      }
      sendResp = 1;
   }
      break;

   case OSIP_NIST_BYE_RECEIVED: {
      int _i = 0;
      osip_dialog_t *dialog = NULL;
      for (_i=0; !osip_list_eol(&_pCtx->osipDialog, _i); ++_i, dialog=NULL) {
         dialog = (osip_dialog_t *)osip_list_get(&_pCtx->osipDialog, _i);
         if (osip_dialog_match_as_uas(dialog, msg) == OSIP_SUCCESS) {
            ESIP_TRACE(ESIP_LOG_DEBUG, "Dialog for FOUND found [STATE:%d]", dialog->state);
            break;
         }
      }

      ESIP_TRACE(ESIP_LOG_INFO,"OSIP_NIST_BYE_RECEIVED");
      if (es_msg_initResponse(&_pResp, SIP_OK, msg) != ES_OK) {
         ESIP_TRACE(ESIP_LOG_ERROR, "Creating Response failed");
         return;
      }
      sendResp = 1;
   }
      break;

   case OSIP_NIST_UNKNOWN_REQUEST_RECEIVED: {
      ESIP_TRACE(ESIP_LOG_INFO,"OSIP_NIST_UNKNOWN_REQUEST_RECEIVED");
   }
      break;

   default: {
      ESIP_TRACE(ESIP_LOG_INFO,"NOT SUPPORTED METHODE");
   }
      break;
   }

   if (sendResp) {
      _pEvt = osip_new_outgoing_sipmessage(_pResp);
      osip_transaction_add_event(tr, _pEvt);

      /* Send notification using event for the transaction */
      if (_es_osip_wakeup(_pCtx) != ES_OK) {
         ESIP_TRACE(ESIP_LOG_ERROR, "sending event failed");
         free(_pEvt);
         return;
      }
   }
}

static es_status _es_osip_set_internal_callbacks(struct es_osip_s *ctx)
{
   osip_t * osip = (osip_t *)0;
   int i = 0;

   ESIP_TRACE(ESIP_LOG_DEBUG,"Enter");

   if (ctx == NULL) {
      ESIP_TRACE(ESIP_LOG_ERROR, "Arguement not valid");
      return ES_ERROR_OUTOFRESOURCES;
   }

   osip = ctx->osip;

   if (osip == NULL) {
      ESIP_TRACE(ESIP_LOG_ERROR, "OSip not initialized");
      return ES_ERROR_OUTOFRESOURCES;
   }

   // callback called when a SIP message must be sent.
   osip_set_cb_send_message(osip, &_es_internal_send_msg_cb);

   // callback called when a SIP transaction is TERMINATED.
   for (i = 0; i < OSIP_KILL_CALLBACK_COUNT; ++i) {
      osip_set_kill_transaction_callback(osip, i, &_es_internal_kill_transaction_cb);
   }

   // callback called when the callback to send message have failed.
   for (i = 0; i < OSIP_TRANSPORT_ERROR_CALLBACK_COUNT; ++i) {
      osip_set_transport_error_callback(osip, i, &_es_internal_transport_error_cb);
   }

   // Message callbacks.
   for (i = 0; i < OSIP_MESSAGE_CALLBACK_COUNT; ++i) {
      osip_set_message_callback(osip, i, &_es_internal_message_cb);
   }

   return ES_OK;
}

// vim: ts=2:sw=2
