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

#include "estransport.h"
#include "esosip.h"

struct es_osip_s {
	/* OSip */
	osip_t 				*osip;
	/* Transport Layer */
	es_transport_t 		*transportCtx;
	/* base loop thread */
	struct event_base 	*base;
	/* Pending Event to handle */
	osip_list_t 		pendingEv;
};

static void _es_transport_event_cb(
		IN es_transport_t 	*transp, 
		IN int 				event, 
		IN int 				error, 
		IN void				*data);

static void _es_transport_msg_cb(
		IN es_transport_t*    	transp,
		IN const char const 	*msg,
		IN const unsigned int 	size,
		OUT void 				*data);

static es_status _es_osip_set_internal_callbacks(
		IN struct es_osip_s *_ctx);

static es_status _es_send_event(
		IN struct es_osip_s *ctx);

es_status es_osip_init(OUT es_osip_t **_ctx, struct event_base *base)
{
	es_status ret = ES_OK;
	struct es_osip_s *ctx = (struct es_osip_s *) malloc(sizeof(struct es_osip_s));
	if (ctx == NULL) {
		ESIP_TRACE(ESIP_LOG_ERROR, "Can not initialize OSip stack: no more memory");
		return ES_ERROR_OUTOFRESOURCES;
	}

	/* Set Magic */
	/* TODO */

	/* Init Transport Layer */
	ret = es_transport_init(&ctx->transportCtx, base);
	if (ret != ES_OK) {
		ESIP_TRACE(ESIP_LOG_ERROR, "Initializing Transport Layer failed");
		free(ctx);
		return ret;
	}

	/* Init Transport Layer Callbacks */
	{
		struct es_transport_callbacks_s cs = {
			&_es_transport_event_cb,
			&_es_transport_msg_cb,
			ctx	
		};

		ret = es_transport_set_callbacks(ctx->transportCtx, &cs);
		if (ret != ES_OK) {
			ESIP_TRACE(ESIP_LOG_ERROR, "Setting Callbacks for Transport Layer failed");
			free(ctx);
			return ret;
		}

		ret = es_transport_start(ctx->transportCtx);
		if (ret != ES_OK) {
			ESIP_TRACE(ESIP_LOG_ERROR, "Start Transport Layer failed");
			free(ctx);
			return ret;
		}
	}

	/* Init OSip */
	if (osip_init(&ctx->osip) != OSIP_SUCCESS) {
		ESIP_TRACE(ESIP_LOG_ERROR, "Can not initialize OSip stack");
		free(ctx);
		return ES_ERROR_UNKNOWN;
	}

	/* list of pending event */
	if (osip_list_init(&ctx->pendingEv) != OSIP_SUCCESS) {
		ESIP_TRACE(ESIP_LOG_ERROR, "List for pending event initialization failed");
		free(ctx->osip);
		free(ctx);
		return ES_ERROR_OUTOFRESOURCES;
	}

	/* Set internal OSip Callback */
	if ((ret = _es_osip_set_internal_callbacks(ctx)) != ES_OK) {
		free(ctx->osip);
		free(ctx);
		return ret;
	}

	/* Set base event thread to use */
	ctx->base = base;

	/* Ok */
	*_ctx = ctx;
	return ES_OK;
}

es_status es_osip_parse_msg(IN es_osip_t *_ctx, const char *buf, unsigned int size)
{
	osip_event_t *evt = NULL;
	struct es_osip_s *ctx = (struct es_osip_s *)_ctx;

	ESIP_TRACE(ESIP_LOG_ERROR, "Enter");

	/* Check Context */
	if (ctx == NULL) {
		ESIP_TRACE(ESIP_LOG_ERROR, "SIP ctx is null");
		return ES_ERROR_NULLPTR;
	}

	/* Check Context magic */
	/* TODO */

	/* Parse buffer and check if it's really a SIP Message */
	evt = osip_parse(buf, size);
	if (evt == NULL) {
		ESIP_TRACE(ESIP_LOG_ERROR, "Error creating OSip event");
		return ES_ERROR_NETWORK_PROBLEM;
	}

	if (osip_find_transaction_and_add_event(ctx->osip, evt) != OSIP_SUCCESS) {
		ESIP_TRACE(ESIP_LOG_INFO, "New transaction");
		osip_transaction_t *tr = NULL;

		/* Init a new INVITE Server Transaction */
		if (evt->type == RCV_REQINVITE) {
			if (osip_transaction_init(&tr, IST, ctx->osip, evt->sip) != OSIP_SUCCESS) {
				ESIP_TRACE(ESIP_LOG_ERROR, "Erro init new transation");
			 	free(evt);
			 	return ES_ERROR_OUTOFRESOURCES;
			}
		} else {
			if (osip_transaction_init(&tr, NIST, ctx->osip, evt->sip) != OSIP_SUCCESS) {
				ESIP_TRACE(ESIP_LOG_ERROR, "Erro init new transation");
			 	free(evt);
			 	return ES_ERROR_OUTOFRESOURCES;
			}
		}

		/* Set Out Socket for the new Transaction, used to send response */
		{
			int fd = -1;
			es_transport_get_udp_socket(ctx->transportCtx, &fd);
			if (osip_transaction_set_out_socket(tr, fd) != OSIP_SUCCESS) {
				ESIP_TRACE(ESIP_LOG_ERROR, "Setting socket descriptor failed");
				free(evt);
				return ES_ERROR_UNKNOWN;
			}
		}

		/* Set context reference into Transaction struct */
		osip_transaction_set_your_instance(tr, (void *)ctx);

		/* add a new OSip event into Fifo list */
		if (osip_transaction_add_event(tr, evt)) {
			ESIP_TRACE(ESIP_LOG_ERROR, "adding event failed");
			free(evt);
			return ES_ERROR_OUTOFRESOURCES;
		}
	}

	/* Send notification using event for the transaction */
	if (_es_send_event(ctx) != ES_OK) {
		ESIP_TRACE(ESIP_LOG_ERROR, "sending event failed");
		free(evt);
		return ES_ERROR_UNKNOWN;
	}

	/* Look for existant Dialog */

	return ES_OK;
}

static void _es_transport_event_cb(
		IN es_transport_t 	*transp, 
		IN int 				event, 
		IN int 				error, 
		IN void 			*data)
{
	ESIP_TRACE(ESIP_LOG_INFO, "Event: %d", event);
}

static void _es_transport_msg_cb(
		IN es_transport_t*    	transp,
		IN const char const 	*msg,
		IN const unsigned int 	size,
		OUT void 				*data)
{
	struct es_osip_s *ctx = (struct es_osip_s *)data;

	if (ctx == NULL) {
		ESIP_TRACE(ESIP_LOG_ERROR, "Context reference is invalid");
		return;
	}

	if (size < 3) {
		ESIP_TRACE(ESIP_LOG_WARNING, "Message length [%d] can not be right !", size);
	}

	ESIP_TRACE(ESIP_LOG_INFO, "Received:\n<=====\n%s\n<=====", msg);

	if (es_osip_parse_msg(ctx, msg, size) != ES_OK) {
		ESIP_TRACE(ESIP_LOG_ERROR, "Error parsing Message!");
		return;
	}
}

static void _es_osip_loop(evutil_socket_t fd, short event, void *arg)
{
	struct es_osip_s *ctx = (struct es_osip_s *)arg;

	ESIP_TRACE(ESIP_LOG_DEBUG, "Enter");

	/* Check Context */
	if (ctx == NULL) {
		ESIP_TRACE(ESIP_LOG_ERROR, "SIP ctx is null");
		return;
	}

	/* Loop until we have no pending event, 
	 * blocking: if there is too mush events 
	 * and we are using the same main loop thread !! */
	while (osip_list_size(&ctx->pendingEv) > 0) {

		/* Get the first event from the list */
		struct event *ev = (struct event *)osip_list_get(&ctx->pendingEv, 0);
		
		ESIP_TRACE(ESIP_LOG_DEBUG, "pending event %p", ev);
		
		/* Remove this event since it handled now */
		osip_list_remove(&ctx->pendingEv, 0);

		/* INVITE Client Transaction Fifo list */
		if (osip_ict_execute(ctx->osip) != OSIP_SUCCESS) {
			ESIP_TRACE(ESIP_LOG_ERROR,"== ICT failed");
		}

		/* INVITE Server Transaction Fifo list */
		if (osip_ist_execute(ctx->osip) != OSIP_SUCCESS) {
			ESIP_TRACE(ESIP_LOG_ERROR,"== IST failed");
		}
		
		/* Non-INVITE Client Transaction Fifo list */
		if (osip_nict_execute(ctx->osip) != OSIP_SUCCESS) {
			ESIP_TRACE(ESIP_LOG_ERROR,"== NICT failed");
		}

		/* Non-INVITE Server Transaction Fifo list */
		if (osip_nist_execute(ctx->osip) != OSIP_SUCCESS) {
			ESIP_TRACE(ESIP_LOG_ERROR,"== NIST failed");
		}

		/* INVITE Client Transation Timer Event Fifo list */
		osip_timers_ict_execute(ctx->osip);

		/* INVITE Server Transaction Timer Event Fifo list */
		osip_timers_ist_execute(ctx->osip);

		/* Non-INVITE Client Transaction Timer Event Fifo list */
		osip_timers_nict_execute(ctx->osip);

		/* Non-INVITE Server Transaction Timer Event Fifo list */
		osip_timers_nist_execute(ctx->osip);
	}
}

static es_status _es_send_event(struct es_osip_s *ctx)
{
	struct event *evsip = NULL;
	
	ESIP_TRACE(ESIP_LOG_ERROR, "Enter");

	/* Check Context */
	if (ctx == NULL) {
		ESIP_TRACE(ESIP_LOG_ERROR, "SIP ctx is null");
		return ES_ERROR_NULLPTR;
	}

	/* Dispatch a new event to unblock the base loop thread */
	evsip = event_new(ctx->base, -1, (EV_READ), _es_osip_loop, (void *)ctx);
	if (evsip == NULL) {
		ESIP_TRACE(ESIP_LOG_ERROR, "Can not create event for OSip stack");
		return ES_ERROR_OUTOFRESOURCES;	
	}

	/* set priority */
	if (event_priority_set(evsip, 0) != 0) {
		ESIP_TRACE(ESIP_LOG_WARNING, "Can not set priority event for OSip stack");
	}

	/* Add the new event to base loop thread handler */
	if (event_add(evsip, NULL) != 0) {
		ESIP_TRACE(ESIP_LOG_ERROR, "Can not make OSip event pending");
		event_del(evsip);
		return ES_ERROR_OUTOFRESOURCES;
	}

	/* This is a pending event now, add it to Fifo list */
	osip_list_add(&ctx->pendingEv, evsip, 0);

	/* activate the event (callabck will be executed) */
	event_active(evsip, EV_READ, 0); 

	return ES_OK;
}

static es_status _es_tools_build_response(
		IN osip_message_t *req,
	    IN const unsigned int code,	
		OUT osip_message_t **resp)
{
	osip_message_t *msg = NULL;
	unsigned int random_tag = 0;
	char str_random[256];

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
	osip_message_init(&msg);

	/* Set SIP Version */
	osip_message_set_version(msg, osip_strdup("SIP/2.0"));

	/* Set status code */
	if (code > 0) {
		osip_message_set_status_code(msg, code);
		osip_message_set_reason_phrase(msg, osip_strdup(osip_message_get_reason(code)));
	}

	/* Set From header */
	osip_from_clone(req->to, &msg->from);

	/* Set To header */
	osip_to_clone(req->from, &msg->to);
	random_tag = osip_build_random_number();
	snprintf(str_random, sizeof(str_random), "%d", random_tag);
	osip_to_set_tag(msg->to, osip_strdup(str_random));

	/* Set CSeq header */
	osip_cseq_clone(req->cseq, &msg->cseq);

	/* Set Call-Id header */
	osip_call_id_clone(req->call_id, &msg->call_id);

	/* Handle Via header */
	{
		int pos = 0;//copy vias from request to response
		while (!osip_list_eol(&req->vias, pos))	{
			osip_via_t *via = NULL;
			osip_via_t *via2 = NULL;

			via = (osip_via_t *) osip_list_get(&req->vias, pos);
			int i = osip_via_clone(via, &via2);
			if (i != 0)	{
				osip_message_free(msg);
				return i;
			}
			osip_list_add(&(msg->vias), via2, -1);
			pos++;
		}
	}

	/* Set User-Agent header */
	osip_message_set_user_agent(msg, osip_strdup(PACKAGE));

	*resp = msg;

	return ES_OK;
}

static int _es_internal_send_msg_cb(
		osip_transaction_t *tr, 
		osip_message_t *msg, 
		char *addr, 
		int port, 
		int socket)
{
	char *buf = NULL;
	size_t buf_len = 0;
	struct es_osip_s *ctx = NULL;

	ctx = osip_transaction_get_your_instance(tr);
	if (ctx == NULL) {
		ESIP_TRACE(ESIP_LOG_ERROR, "Reference is invalid");
		return -1;
	}

	osip_message_to_str(msg, &buf, &buf_len);

	ESIP_TRACE(ESIP_LOG_DEBUG,"Sending \n=====>\n%s\n=====>", buf);

	es_transport_send(ctx->transportCtx, addr, port, buf, buf_len);

	free(buf);

	return OSIP_SUCCESS;
}

static void _es_internal_transport_error_cb(int type, osip_transaction_t *tr, int error)
{
	ESIP_TRACE(ESIP_LOG_INFO,"Error Transport for Transaction %p type %d, error %d", tr, type, error);
}

static void _es_internal_kill_transaction_cb(int type, osip_transaction_t *tr)
{
	struct es_osip_s *ctx = NULL;
	ESIP_TRACE(ESIP_LOG_INFO,"Removing Transaction %p", tr);

	ctx = osip_transaction_get_your_instance(tr);
	if (ctx != NULL) {
		if (osip_remove_transaction (ctx->osip, tr) != OSIP_SUCCESS) {
			  ESIP_TRACE(ESIP_LOG_ERROR, "Remove Transaction %p failed", tr);
		}
	}
}

static void _es_internal_message_cb(int type, osip_transaction_t *tr, osip_message_t *msg)
{
	struct es_osip_s *ctx = NULL;
	osip_message_t *response = NULL;        
	osip_event_t *evt = NULL;
	
	ESIP_TRACE(ESIP_LOG_INFO,"Enter: type %d", type);

	ctx = osip_transaction_get_your_instance(tr);
	if (ctx == NULL) {
		ESIP_TRACE(ESIP_LOG_ERROR, "Reference is invalid");
		return;
	}

	if (_es_tools_build_response(msg, 0, &response) != ES_OK) {
		ESIP_TRACE(ESIP_LOG_ERROR, "Creating Response failed");
		return;
	}

	switch (type) {
		case OSIP_IST_INVITE_RECEIVED:
			{
				ESIP_TRACE(ESIP_LOG_INFO,"OSIP_IST_INVITE_RECEIVED");

				osip_message_set_status_code(response, SIP_TRYING);
				osip_message_set_reason_phrase(response, osip_strdup("Trying"));
			}
			break;
		case OSIP_NIST_REGISTER_RECEIVED:
			{
				/* TODO: Send it to REGISTRAR module */
				ESIP_TRACE(ESIP_LOG_INFO,"OSIP_NIST_REGISTER_RECEIVED");

				osip_message_set_status_code(response, SIP_OK);
				osip_message_set_reason_phrase(response, osip_strdup("OK"));
			}
			break;
		default:
			{
				ESIP_TRACE(ESIP_LOG_INFO,"NOT SUPPORTED METHODE");
				osip_message_set_status_code(response, SIP_NOT_IMPLEMENTED);
				osip_message_set_reason_phrase(response, osip_strdup("Not Implemented Methode"));
			}
			break;
	}

	evt = osip_new_outgoing_sipmessage(response);
	osip_transaction_add_event(tr, evt);

	/* Send notification using event for the transaction */
	if (_es_send_event(ctx) != ES_OK) {
		ESIP_TRACE(ESIP_LOG_ERROR, "sending event failed");
		free(evt);
		return;
	}
}

static es_status _es_osip_set_internal_callbacks(struct es_osip_s *ctx)
{
	osip_t *osip = NULL;
	
	ESIP_TRACE(ESIP_LOG_INFO,"Enter");
	
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
	osip_set_kill_transaction_callback(osip, OSIP_ICT_KILL_TRANSACTION, &_es_internal_kill_transaction_cb);
	osip_set_kill_transaction_callback(osip, OSIP_NIST_KILL_TRANSACTION, &_es_internal_kill_transaction_cb);
	osip_set_kill_transaction_callback(osip, OSIP_NICT_KILL_TRANSACTION, &_es_internal_kill_transaction_cb);
	osip_set_kill_transaction_callback(osip, OSIP_NIST_KILL_TRANSACTION, &_es_internal_kill_transaction_cb);
	
	// callback called when the callback to send message have failed.
	osip_set_transport_error_callback(osip,	OSIP_ICT_TRANSPORT_ERROR, &_es_internal_transport_error_cb);
	osip_set_transport_error_callback(osip, OSIP_IST_TRANSPORT_ERROR, &_es_internal_transport_error_cb);
	osip_set_transport_error_callback(osip, OSIP_NICT_TRANSPORT_ERROR, &_es_internal_transport_error_cb);
	osip_set_transport_error_callback(osip,	OSIP_NIST_TRANSPORT_ERROR, &_es_internal_transport_error_cb);
	
	// callback called when a received answer has been accepted by the transaction.
	osip_set_message_callback(osip ,OSIP_ICT_STATUS_1XX_RECEIVED, &_es_internal_message_cb);
	osip_set_message_callback(osip ,OSIP_ICT_STATUS_2XX_RECEIVED, &_es_internal_message_cb);
	osip_set_message_callback(osip ,OSIP_ICT_STATUS_3XX_RECEIVED, &_es_internal_message_cb);
	osip_set_message_callback(osip ,OSIP_ICT_STATUS_4XX_RECEIVED, &_es_internal_message_cb);
	osip_set_message_callback(osip ,OSIP_ICT_STATUS_5XX_RECEIVED, &_es_internal_message_cb);
	osip_set_message_callback(osip ,OSIP_ICT_STATUS_6XX_RECEIVED, &_es_internal_message_cb);

	// callback called when a received answer has been accepted by the transaction.
	osip_set_message_callback(osip ,OSIP_NICT_STATUS_1XX_RECEIVED, &_es_internal_message_cb);
	osip_set_message_callback(osip ,OSIP_NICT_STATUS_2XX_RECEIVED, &_es_internal_message_cb);
	osip_set_message_callback(osip ,OSIP_NICT_STATUS_3XX_RECEIVED, &_es_internal_message_cb);
	osip_set_message_callback(osip ,OSIP_NICT_STATUS_4XX_RECEIVED, &_es_internal_message_cb);
	osip_set_message_callback(osip ,OSIP_NICT_STATUS_5XX_RECEIVED, &_es_internal_message_cb);
	osip_set_message_callback(osip ,OSIP_NICT_STATUS_6XX_RECEIVED, &_es_internal_message_cb);

	// callback called when a received request has been accepted by the transaction.
	osip_set_message_callback(osip ,OSIP_IST_INVITE_RECEIVED,     &_es_internal_message_cb);
	osip_set_message_callback(osip ,OSIP_IST_ACK_RECEIVED,        &_es_internal_message_cb);
	osip_set_message_callback(osip ,OSIP_NIST_REGISTER_RECEIVED,  &_es_internal_message_cb);
	osip_set_message_callback(osip ,OSIP_NIST_BYE_RECEIVED,       &_es_internal_message_cb);
	osip_set_message_callback(osip ,OSIP_NIST_CANCEL_RECEIVED,    &_es_internal_message_cb);
	osip_set_message_callback(osip ,OSIP_NIST_INFO_RECEIVED,      &_es_internal_message_cb);
	osip_set_message_callback(osip ,OSIP_NIST_OPTIONS_RECEIVED,   &_es_internal_message_cb);
	osip_set_message_callback(osip ,OSIP_NIST_SUBSCRIBE_RECEIVED, &_es_internal_message_cb);
	osip_set_message_callback(osip ,OSIP_NIST_NOTIFY_RECEIVED,    &_es_internal_message_cb);
	osip_set_message_callback(osip ,OSIP_NIST_UNKNOWN_REQUEST_RECEIVED, &_es_internal_message_cb);
	
	return ES_OK;
}

