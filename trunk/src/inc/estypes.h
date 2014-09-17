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

#ifndef _ESIP_TYPES_H_
#define _ESIP_TYPES_H_

#include <stddef.h>

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#if HAVE_STDINT_H
#include <stdint.h>
#elif HAVE_INTTYPES_H
#include <inttypes.h>
#else
#error Define HAVE_STDINT_H as 1 if you have <stdint.h>, \
   or HAVE_INTTYPES_H if you have <inttypes.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#define ES_MIN(_a, _b) (((_a) < (_b) ? (_a) : (_b)))
#define ES_MAX(_a, _b) (((_a) > (_b) ? (_a) : (_b)))

#ifdef __GNUC__
# define UNUSED(d) d __attribute__ ((unused))
#else
# define UNUSED(d) d
#endif

#define IN
#define OUT
#define INOUT

#define VALUE2STRING(_m)  #_m
#define MACRO2STRING(_m)  VALUE2STRING(_m)


#if defined(__cplusplus)
}
#endif
#endif /* _ESIP_TYPES_H_ */
