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

#define _GNU_SOURCE

#include "configuration.h"
#include "macros.h"

#include <stdio.h>	/* getline()	*/
#include <stdlib.h>	/* atof()	*/

#include "error.h"
#include "predictor.h"

static inline void getresult(double *array, size_t array_len, size_t length) {
	predanswer_t *answer = predictor(length, &array[array_len - length]);
	critical_on (answer == NULL);
	printf("Result %4i:\t%3lf\t(approximated_currency: %lf)\n", length, answer->to_buy, answer->approximated_currency);
}

int main() {
	double array[PARSER_MAXELEMENTS];
	size_t array_len = 0;

	// Initializing output subsystem
	{
		int output_method     = OM_STDERR;
		int output_quiet      = 0;
		int output_verbosity  = 9;
		int output_debuglevel = 9;

		error_init(&output_method, &output_quiet, &output_verbosity, &output_debuglevel);
	}

	// Parsing the input
	{
		char   *line     = NULL;
		size_t  line_len = 0;
		ssize_t read_len;
		while ((read_len = getline(&line, &line_len, stdin)) != -1) {
			double value;

			while (1) {
				char c;

				if (!read_len)
					break;

				c = line[read_len - 1];
				if (c >= '0' && c <= '9')
					break;
				read_len--;
			}
			if (!read_len)	/* just skip empty line */
				continue;

			line[read_len] = 0;

			value = atof(line);
			debug(10, "Line: %lf (%li, %s)", value, read_len, line);
			array[ array_len++ ] = value;
		}
	}

	printf("The last cost is %lf\n", array[array_len-1]);

	getresult(array, array_len, 7);
	getresult(array, array_len, 15);
	getresult(array, array_len, 31);
	getresult(array, array_len, 75);
	getresult(array, array_len, 365);
	getresult(array, array_len, 901);

	return 0;
}

