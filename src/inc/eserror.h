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

#ifndef _ESIP_ERROR_H_
#define _ESIP_ERROR_H_

#include "estypes.h"

typedef int es_status;

#define ES_OK 0

/* Common Error Codes -1..-511 */
#define ES_ERROR_UNKNOWN -1                 /* There was an error, but we don't know specifics */
#define ES_ERROR_OUTOFRESOURCES -2          /* no resource left for this operation */
#define ES_ERROR_BADPARAM -3                /* parameter value invalid */
#define ES_ERROR_NULLPTR -4                 /* required pointer parameter was a NULL pointer */
#define ES_ERROR_OUTOFRANGE -5              /* parameter out of range */
#define ES_ERROR_DESTRUCTED -6              /* operation attempted on a destructed object */
#define ES_ERROR_NOTSUPPORTED -7            /* request not supported under current configuration */
#define ES_ERROR_UNINITIALIZED -8           /* object uninitialized */
#define ES_ERROR_TRY_AGAIN -9	              /* incomplete operation, used by semaphore's try lock */
#define ES_ERROR_ILLEGAL_ACTION -10         /* the requested action is illegal */
#define ES_ERROR_NETWORK_PROBLEM -11        /* action failed due to network problems */
#define ES_ERROR_INVALID_HANDLE -12         /* a handle passed to a function is illegal */
#define ES_ERROR_NOT_FOUND -13              /* the requested item cannot be found */
#define ES_ERROR_INSUFFICIENT_BUFFER -14    /* the buffer is too small */

/* Common Warning Codes 1..511 */
/* None yet.*/

#endif /* _ESIP_ERROR_H_ */
