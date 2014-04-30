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
#define ESIP_LOG_EMERG		0
#define ESIP_LOG_ALERT		1
#define ESIP_LOG_CRIT		2
#define ESIP_LOG_ERROR		3
#define ESIP_LOG_WARNING	4
#define ESIP_LOG_NOTICE		5
#define ESIP_LOG_INFO		6
#define ESIP_LOG_DEBUG		7

/** */
#define ESIP_TRACE(level, msg, ...)	\
	do { \
		printf("[%s] %s(%d) - " msg "\r\n", PACKAGE, __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__); \
	} while(0)

#endif /* _ESIP_LOG_H_ */
