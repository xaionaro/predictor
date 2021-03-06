/*
    predictor
    
    Copyright (C) 2014 Dmitry Yu Okunev <dyokunev@ut.mephi.ru> 0x8E30679C
    
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#define	SYSLOG_BUFSIZ			4096
#define	SYSLOG_FLAGS			LOG_PID
#define	SYSLOG_FACILITY			LOG_INFO

#define	OUTPUT_LOCK_TIMEOUT		1000

#define PARSER_MAXELEMENTS		(86401 * 92)
#define PARSER_MAXORDERS		1

#define PREDICTOR_APPROXIMATE_LEN	(3600 * 24 * 365 + 1)

