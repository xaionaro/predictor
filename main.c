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
#include <math.h>

#include "error.h"
#include "predictor.h"

static inline double getresult(double *array, size_t array_len, size_t length, predanswer_t **answer_p) {
	if (length > array_len)
		length = array_len;

	predanswer_t *answer = predictor(length, &array[array_len - length]);
	critical_on (answer == NULL);
	fprintf(stderr, "Result %4li:\t%3lf\t(approximated_currency: %lf)\tsqdiff: %lf\tc[0]: %lf\tc[1]: %lf\tc[2]: %lf\n", length, answer->to_buy, answer->approximated_currency, answer->sqdiff, answer->c[0], answer->c[1], answer->c[2]);
	*answer_p = answer;
	return answer->to_buy;
}

int main(int argc, char *argv[]) {
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
			debug(8, "Line: %lf (%li, %s)", value, read_len, line);
			array[ array_len++ ] = value;
		}
	}

	fprintf(stderr, "The last cost is %lf\n", array[array_len-1]);

	double to_buy = 0, to_buy_next, sign, c0_7, c1, c1_7, sqdiff_7;
	char pass = 1;
	int negativec1 = 0, negativec2 = 0;
	predanswer_t *answer;

	c1 = -100000;

	to_buy_next   = getresult(array, array_len, 7,		&answer)*500;
	sign = to_buy_next;
	to_buy += to_buy_next;
	fprintf(stderr, "to_buy == %lf\n", to_buy);
	if (c1 < answer->c[1]) {
		negativec1 += (answer->c[1]<0);
		c1          = answer->c[1];
	}
	negativec2 += (answer->c[2]<0);
	c0_7		    = answer->c[0];
	c1_7		    = answer->c[1];
	sqdiff_7	    = answer->sqdiff;

	to_buy_next   = getresult(array, array_len, 15,		&answer)*500;
	if (sign*to_buy_next < 0) pass = 0;
	to_buy += to_buy_next;
	fprintf(stderr, "to_buy == %lf\n", to_buy);
	if (c1 < answer->c[1]) {
		negativec1 += (answer->c[1]<0);
		c1          = answer->c[1];
	}
	negativec2 += (answer->c[2]<0);

	to_buy_next   = getresult(array, array_len, 31,		&answer)*700;
	if (sign*to_buy_next < 0) pass = 0;
	to_buy += to_buy_next;
	fprintf(stderr, "to_buy == %lf\n", to_buy);
	if (c1 < answer->c[1]) {
		negativec1 += (answer->c[1]<0);
		c1          = answer->c[1];
	}
	negativec2 += (answer->c[2]<0);

	to_buy_next   = getresult(array, array_len, 75,		&answer)*1000;
//	if (sign*to_buy_next < 0) pass = 0;
	to_buy += to_buy_next;
	fprintf(stderr, "to_buy == %lf\n", to_buy);
	if (c1 < answer->c[1]) {
		negativec1 += (answer->c[1]<0);
		c1          = answer->c[1];
	}
	negativec2 += (answer->c[2]<0);

	to_buy_next   = getresult(array, array_len, 365,	&answer)*1000;
//	if (sign*to_buy_next < 0) pass = 0;
	to_buy += to_buy_next;
	fprintf(stderr, "to_buy == %lf\n", to_buy);
	if (c1 < answer->c[1]) {
		negativec1 += (answer->c[1]<0);
		c1          = answer->c[1];
	}
	negativec2 += (answer->c[2]<0);
/*	to_buy_next   = getresult(array, array_len, 901);
	if (sign*to_buy_next < 0) pass = 0;
	to_buy += to_buy_next;*/

	// For bad compaines (fuse), but it's bad for LJPC and doesn't help against YNDX, too
	{
		if (((negativec1 + negativec2) >= 6) && (c1_7 < 0))
			to_buy -= -c1_7/answer->c[0]/sqdiff_7/sqdiff_7/sqdiff_7/answer->sqdiff/9000000;

		to_buy *= pass;

		if (((array[array_len - 1] - array[array_len - 2]) < 0) && ((array[array_len - 2] - array[array_len - 3] ) < 0) && ((array[array_len - 3] - array[array_len - 4]) < 0) && ((array[array_len - 4] - array[array_len - 5]) < 0))
			to_buy = -9999;
	}

	printf("%lf\n", to_buy);

	return 0;
}

