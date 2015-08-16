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
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <assert.h>

#include "malloc.h"
#include "error.h"
#include "predictor.h"

static inline int split(char **r, char *line, char sep, int fields) {
	char *ptr;
	int fields_count = 0;

	if (fields < 1)
		goto l_split_end;

	r[fields_count++] = ptr = line;

	while (1) {
		switch (*ptr) {
			case 0:
			case '\n':
			case '\r':
				goto l_split_end;
			default:
				if (*ptr == sep) {
					*ptr = 0;
					r[fields_count++] = ptr+1;
				}
		}

		ptr++;
	}

l_split_end:

	return fields_count;
}

static inline double getresult(double *array, size_t array_len, size_t length, predanswer_t **answer_p) {
	if (length > array_len)
		length = array_len;

	predanswer_t *answer = predictor(length, array);
	critical_on (answer == NULL);
	fprintf(stderr, "Result %4li:\t%3lf\t(approximated_currency: %lf)\tsqdiff: %lf\tc[0]: %lf\tc[1]: %lf\tc[2]: %lf\n", length, answer->to_buy, answer->approximated_currency, answer->sqdiff, answer->c[0], answer->c[1], answer->c[2]);
	*answer_p = answer;
	return answer->to_buy;
}

static inline int set_currency(double *array, uint32_t ts_prev, uint32_t ts, double currency) {
	uint32_t ts_diff = ts - ts_prev;

	if ( ts >= PARSER_MAXELEMENTS )
		return ENOMEM;

	//debug(7, "currency == %lf\n", currency);

	assert ( ts_prev <= ts );

	double currency_prev = array[ts_prev];
	uint32_t i = 0;
	while (++i < ts_diff) {
		array[ ts_prev+i ] = currency_prev + ((currency - currency_prev) * (double)i/(double)ts_diff);
	}
	array[ ts ] = currency;

	return 0;
}

int main(int argc, char *argv[]) {
	double **array = xmalloc(PARSER_MAXORDERS * sizeof(double *));
	size_t array_len[PARSER_MAXORDERS] = {0};

	// Initializing output subsystem
	{
		static int output_method     = OM_STDERR;
		static int output_quiet      = 0;
		static int output_verbosity  = 9;
		static int output_debuglevel = 9;

		error_init(&output_method, &output_quiet, &output_verbosity, &output_debuglevel);
	}

	// Initializing the array
	{
		int i = 0;
		while (i < PARSER_MAXORDERS) {
			array[i++] = xmalloc(PARSER_MAXELEMENTS * sizeof(double));
		}
	}

	// Parsing the input
	{
		int order;
		double currency = 0;
		uint32_t ts_orig = ~0;
		uint32_t ts_first[PARSER_MAXORDERS];
		uint32_t ts_prev[PARSER_MAXORDERS];
		char   *line     = NULL;
		size_t  line_len = 0;
		ssize_t read_len;
		int curcount[PARSER_MAXORDERS];
		uint32_t curcurrency[PARSER_MAXORDERS];
		char **words = (char **)malloc(2 * sizeof(char*));
		uint32_t ts_prev_prev[PARSER_MAXORDERS];

		order = 0;
		while (order < PARSER_MAXORDERS) {
			ts_first[order]		= ~0;
			ts_prev[order]		= ~0;
			ts_prev_prev[order]	= ~0;
			curcount[order]		=  1;
			curcurrency[order]	=  0;
			array_len[order]	=  0;

			order++;
		}

		while ((read_len = getline(&line, &line_len, stdin)) != -1) {
			int fields_count = split (words, line, ',', 2);
			if (fields_count < 2) {
				fprintf(stderr, "Not enough fields: %i. Skipping...\n", fields_count);
				continue;
			}
			ts_orig  = atoi(words[0]);
			currency = atof(words[1]);

			//fprintf(stderr, "currency == %lf\n", currency);

			if (ts_orig == 0) {
				fprintf(stderr, "Warning: ts == %u. Skipping...\n", ts_orig);
				continue;
			}

			if (ts_orig > ts_prev[0]) {
				fprintf(stderr, "Warning: %u is greater than %u. Skipping...\n", ts_orig, ts_prev[0]);
				continue;
			}

			order = 0;
			do {
				uint32_t ts;
				ts = ts_orig >> order;

				if (ts != ts_prev[order]) {
					if (ts_first[order] == ~0) {
						ts_prev_prev[order] = ts;
						ts_prev[order]      = ts;
						ts_first[order]     = ts;
						debug(6, "ts_first == %u (ts_prev_prev == %u)", ts, ts_prev_prev[order]);
					}
					//debug(6, "ts_first == %u; ts_prev_prev == %u; ts_prev == %u", ts, ts_prev_prev, ts_prev);

					debug(7, "curcurrency[%i] == %lf (%u)", order, curcurrency[order], ts);

					if (set_currency (array[order], ts_first[order] - ts_prev_prev[order], ts_first[order] - ts_prev[order], curcurrency[order]))
						continue;

					array_len[order] = ts_first[order] - ts_prev[order];

					curcount[order]    = 1;
					curcurrency[order] = currency;

					ts_prev_prev[order] = ts_prev[order];
				} else

				if (ts == ts_prev[order]) {
					curcount[order]++;
//					fprintf(stderr, "curcurrency[%i] ==> %lf\n", order, curcurrency[order]);
					curcurrency[order] = (curcurrency[order]*((double)curcount[order]-1) + currency) / (double)curcount[order];	// average
//					fprintf(stderr, "curcurrency[%i] <== %lf\n", order, curcurrency[order]);
				}

				ts_prev[order] = ts;
			} while (++order < PARSER_MAXORDERS);

			if (ts_first[0] != ~0) {
				if (((ts_first[0] - ts_orig) >> (PARSER_MAXORDERS-1)) > PARSER_MAXELEMENTS) {
					break;
				}
			}
		}
		free (words);

		order = 0;
		do {
			uint32_t ts;
			ts = ts_orig >> order;

			if (!set_currency (array[order], ts_first[order] - ts_prev[order], ts_first[order] - ts, curcurrency[order]))
				array_len[order] = ts_first[order] - ts;

			debug(1, "array_len[%u] == %u (%u - %u + 1)", order, array_len[order], ts_first[order], ts);
		} while (++order < PARSER_MAXORDERS);
	}

	{
		uint32_t i = 0;
		while (i < array_len[0]) {
			debug(8, "%i %lf", i, array[0][i]);
			i++;
		}
	}

	fprintf(stderr, "The last cost is %lf\n", array[0][array_len[0]-1]);

	double to_buy = 0, to_buy_next, sign, c0_7, c1, c1_7, sqdiff_7;
	char pass = 1;
	int negativec1 = 0, negativec2 = 0;
	predanswer_t *answer;

	c1 = -100000;

	to_buy_next = getresult(array[0], array_len[0], 7, &answer);
	sign = to_buy_next;
	to_buy += to_buy_next;
	fprintf(stderr, "to_buy == %lf\n", to_buy);

/*
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
/ *	to_buy_next   = getresult(array, array_len, 901);
	if (sign*to_buy_next < 0) pass = 0;
	to_buy += to_buy_next;* /

	// For bad compaines (fuse), but it's bad for LJPC and doesn't help against YNDX, too
	{
		if (((negativec1 + negativec2) >= 6) && (c1_7 < 0))
			to_buy -= -c1_7/answer->c[0]/sqdiff_7/sqdiff_7/sqdiff_7/answer->sqdiff/9000000;

		to_buy *= pass;

		if (((array[array_len - 1] - array[array_len - 2]) < 0) && ((array[array_len - 2] - array[array_len - 3] ) < 0) && ((array[array_len - 3] - array[array_len - 4]) < 0) && ((array[array_len - 4] - array[array_len - 5]) < 0))
			to_buy = -9999;
	}
*/
	printf("%lf\n", to_buy);

	// Freeing the array
	{
		int i = 0;
		while (i < PARSER_MAXORDERS) {
			free(array[i++]);
		}
		free(array);
	}

	return 0;
}

