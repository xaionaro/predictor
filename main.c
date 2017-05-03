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
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>


#include "malloc.h"
#include "error.h"
#include "predictor.h"

double bet;
int analyzeperiod;
int parse_analyzeperiod;
double require_result      = 1.0040;
double require_result_buy  = 1.1000;
double require_result_sell = 1.1000;

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
					if (fields_count < fields)
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

	predanswer_t *answer;

	answer = predictor(length, array);
	critical_on (answer == NULL);

	//fprintf(stderr, "Result %4li:\t%3lf\t(approximated_currency: %lf)\tsqdiff: %lf\tc[0]: %lf\tc[1]: %lf\tc[2]: %lf\n", length, answer->to_buy, answer->approximated_currency, answer->sqdiff, answer->c[0], answer->c[1], answer->c[2]);
	*answer_p = answer;

	double to_buy  = answer->approximated_currency - array[0];

//	if (fabs(to_buy) / array[0] < 0.004)
//		return 0;

	//fprintf(stderr, "|%lf|", to_buy);
	return to_buy;
}

static inline int set_currency(double *array, uint32_t ts_prev, uint32_t ts, double currency) {
	uint32_t ts_diff = ts - ts_prev;

	if ( ts >= parse_analyzeperiod )
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

double get_to_buy(double *array, size_t array_len, size_t mul) {
	predanswer_t *answer;
	if (analyzeperiod > 0) {
		//return getresult(array, array_len, analyzeperiod, &answer)*(double)10/sqrt((double)analyzeperiod/(double)60/(double)24)/array[0] * mul*(bet*250)/analyzeperiod;
		return getresult(array, array_len, analyzeperiod, &answer);
	}

	double to_buy = 0, to_buy_next, sign, /*c0_7,*/ c1, c1_7, sqdiff_7;
	char pass = 1;
	int negativec1 = 0, negativec2 = 0;

	c1 = -100000;

	fprintf(stderr, "array[0] == %lf\t", array[0]);

	to_buy_next = getresult(array, array_len, (60)+1,    &answer)*(double)3/sqrt((double)1/(double)60/(double)24)/array[0];

	fprintf(stderr, "k == %lf\t", (answer->approximated_currency - array[0]) / array[0]);
	if ((answer->approximated_currency - array[0]) / array[0] > (require_result_buy-1))
		return 0.1;

	sign = to_buy_next;
	to_buy += to_buy_next;
	fprintf(stderr, "to_buy == %lf (%lf)\t", to_buy, answer->approximated_currency);

	to_buy_next = getresult(array, array_len, (3600*24)+1, &answer)*(double)2/sqrt(1)/array[0];

	fprintf(stderr, "k == %lf\t", (answer->approximated_currency - array[0]) / array[0]);
	if ((answer->approximated_currency - array[0]) / array[0] > (require_result_buy-1))
		return 0.1;

	sign = to_buy_next;
	to_buy += to_buy_next;
	fprintf(stderr, "to_buy == %lf (%lf)\t", to_buy, answer->approximated_currency);

	if (c1 < answer->c[1]) {
		negativec1 += (answer->c[1]<0);
		c1          = answer->c[1];
	}
	negativec2 += (answer->c[2]<0);
	//c0_7		    = answer->c[0];
	c1_7		    = answer->c[1];
	sqdiff_7	    = answer->sqdiff;

	to_buy_next   = getresult(array, array_len, 7*(3600*24)+1,		&answer)/sqrt(7)/array[0];
	if (sign*to_buy_next < 0) pass = 0;
	to_buy += to_buy_next;
	fprintf(stderr, "to_buy == %lf (%lf)\t", to_buy, answer->approximated_currency);
	if (c1 < answer->c[1]) {
		negativec1 += (answer->c[1]<0);
		c1          = answer->c[1];
	}
	negativec2 += (answer->c[2]<0);

	to_buy_next   = getresult(array, array_len, 31*(3600*24)+1,		&answer)/sqrt(31)/array[0];
	if (sign*to_buy_next < 0) pass = 0;
	to_buy += to_buy_next;
	fprintf(stderr, "to_buy == %lf (%lf)\t", to_buy, answer->approximated_currency);
	if (c1 < answer->c[1]) {
		negativec1 += (answer->c[1]<0);
		c1          = answer->c[1];
	}
	negativec2 += (answer->c[2]<0);

	to_buy_next   = getresult(array, array_len, 75*(3600*24)+1,		&answer)/sqrt(75)/array[0];
//	if (sign*to_buy_next < 0) pass = 0;
	to_buy += to_buy_next;
	fprintf(stderr, "to_buy == %lf (%lf)\t", to_buy, answer->approximated_currency);
	if (c1 < answer->c[1]) {
		negativec1 += (answer->c[1]<0);
		c1          = answer->c[1];
	}
	negativec2 += (answer->c[2]<0);

	to_buy_next   = getresult(array, array_len, 365*(3600*24)+1,		&answer)/sqrt(365)/array[0];
//	if (sign*to_buy_next < 0) pass = 0;
	to_buy += to_buy_next;
	fprintf(stderr, "to_buy == %lf\t", to_buy);
	if (c1 < answer->c[1]) {
		negativec1 += (answer->c[1]<0);
		c1          = answer->c[1];
	}
	negativec2 += (answer->c[2]<0);
/*	to_buy_next   = getresult(array, array_len, 901);
	if (sign*to_buy_next < 0) pass = 0;
	to_buy += to_buy_next;*/
	fprintf(stderr, "to_buy == %lf\t", to_buy);

	// For bad compaines (fuse), but it's bad for LJPC and doesn't help against YNDX, too
	if (0){
		if (((negativec1 + negativec2) >= 6) && (c1_7 < 0))
			to_buy -= -c1_7/answer->c[0]/sqdiff_7/sqdiff_7/sqdiff_7/answer->sqdiff/9000000;

		to_buy *= pass;

		if (((array[array_len - 1] - array[array_len - 2]) < 0) && ((array[array_len - 2] - array[array_len - 3] ) < 0) && ((array[array_len - 3] - array[array_len - 4]) < 0) && ((array[array_len - 4] - array[array_len - 5]) < 0))
			to_buy = -9999;
	}
	fprintf(stderr, "to_buy == %lf\n", to_buy);

	return mul*to_buy*(bet*250)/450000;
}

void readarray(double **array, size_t *array_len, const char const *fpath)
{
	{
		int order;
		double currency = 0;
		uint32_t ts_orig = ~0;
		uint32_t ts_first[PARSER_MAXORDERS];
		uint32_t ts_prev[PARSER_MAXORDERS];
		//char   *line     = NULL;
		//size_t  line_len = 0;
		//ssize_t read_len;
		int curcount[PARSER_MAXORDERS];
		double curcurrency[PARSER_MAXORDERS];
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

		char   line[BUFSIZ];
		struct stat st;
		assert (stat(fpath, &st) != -1);

		int fd;
		char *data;

		fd = open(fpath, O_RDONLY);

		assert (fd > 0);

		data = mmap((caddr_t)0, st.st_size, PROT_READ, MAP_SHARED, fd, 0);

		assert (data != (caddr_t)(-1));

		size_t pos = st.st_size-1;

		debug(8, "st.st_size == %u; pos == %u", st.st_size, pos);
		//while ((read_len = getline(&line, &line_len, stdin)) != -1) {
		while (1) {
			pos--;
			size_t oldpos = pos;
			while (data[pos] != '\n' && pos > 0) pos--;
			assert (pos > 0);
			pos++;

			assert (oldpos - pos < BUFSIZ-1);

			debug(8, "pos == %lu; oldpos == %lu", pos, oldpos);

			memcpy(line, &data[pos], oldpos - pos);

			line [oldpos - pos] = 0;

			debug(8, "line == %s", line);

			pos--;

			int fields_count = split (words, line, ',', 2);
			if (fields_count < 2) {
				fprintf(stderr, "Not enough fields: %i. Skipping...\n", fields_count);
				continue;
			}
			ts_orig  = atoi(words[0]);
			currency = atof(words[1]);

			debug(8, "ts_orig == %u; currency == %lf", ts_orig, currency);

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
					debug(6, "ts_first == %u; ts_prev_prev == %u; ts_prev == %u", ts, ts_prev_prev, ts_prev);

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
					debug(8, "curcurrency[%i] ==> %lf", order, curcurrency[order]);
					curcurrency[order] = (curcurrency[order]*((double)curcount[order]-1) + currency) / (double)curcount[order];	// average
					debug(8, "curcurrency[%i] <== %lf (%i)", order, curcurrency[order], curcount[order]);
				}

				ts_prev[order] = ts;
			} while (++order < PARSER_MAXORDERS);

			if (ts_first[0] != ~0) {
				if (((ts_first[0] - ts_orig) >> (PARSER_MAXORDERS-1)) > parse_analyzeperiod) {
					break;
				}
			}
		}
		free (words);

		close(fd);
		//munmap(); // TODO: this

		order = 0;
		do {
			uint32_t ts;
			ts = ts_orig >> order;

			if (!set_currency (array[order], ts_first[order] - ts_prev[order], ts_first[order] - ts, curcurrency[order]))
				array_len[order] = ts_first[order] - ts;

			debug(1, "array_len[%u] == %u (%u - %u + 1)", order, array_len[order], ts_first[order], ts);
		} while (++order < PARSER_MAXORDERS);
	}

	if (0) {
		uint32_t i = 0;
		while (i < array_len[0]) {
			debug(8, "result: %i %lf", i, array[0][i]);
			i++;
		}
	}

	//fprintf(stderr, "The last cost is %lf\n", array[0][array_len[0]-1]);

	return;
}

int main(int argc, char *argv[]) {
	double **    array = xmalloc(PARSER_MAXORDERS * sizeof(double *));
	double **oil_array = xmalloc(PARSER_MAXORDERS * sizeof(double *));
	size_t     array_len[PARSER_MAXORDERS] = {0};
	size_t oil_array_len[PARSER_MAXORDERS] = {0};
	double oil_k;

	// Initializing output subsystem
	{
		static int output_method     = OM_STDERR;
		static int output_quiet      = 0;
		static int output_verbosity  = 9;
		static int output_debuglevel = 9;

		error_init(&output_method, &output_quiet, &output_verbosity, &output_debuglevel);
	}

	{
		if (argc <= 5) {
			critical("Not enough arguments");
		}

		bet = atof (argv[1]);

		oil_k               = atoi (argv[3]);
		analyzeperiod       = atoi (argv[5]);
		parse_analyzeperiod = (analyzeperiod ? analyzeperiod : PARSER_MAXELEMENTS);
	}

	// Initializing the array
	{
		int i = 0;
		while (i < PARSER_MAXORDERS) {
			    array[i] = xmalloc(parse_analyzeperiod * sizeof(double));
			oil_array[i] = xmalloc(parse_analyzeperiod * sizeof(double));
			i++;
		}
	}

	// Parsing the input
	readarray(array,     array_len,     argv[2]);
	readarray(oil_array, oil_array_len, argv[4]);

	debug(4, "array[0][0] == %lf; oil_array[0][0] == %lf (%lf)", array[0][0], oil_array[0][0], oil_k*oil_array[0][0]);

	{
		int i = 0;
		while (i < array_len[0]) {
			array[0][i] += oil_k * oil_array[0][i];
			i++;
		}
	}

	debug(4, "array[0][0] == %lf", array[0][0]);

	size_t check_history_orig = 0;//30;
	size_t check_length       = 3600*24*1;
	size_t check_interval     = 10;
	int check_i = 0;

	if (check_history_orig) {

		double k_sum = 0;

		size_t check_history = check_history_orig;

		double money_t = 0.5;
		double value_t = 0.5/array[0][check_length * check_history];
		double buy_cost_avg_t = array[0][check_length * check_history];

		int blocked = 0;

		//#pragma omp parallel for
		for (size_t check_history=check_history_orig; check_history > 0; check_history--) {
		//while (1) {
			size_t check_since = check_length * check_history;
			if ((check_history-1) <= 0)
				break;

			double money = 0.5;
			double value = 0.5/array[0][check_length * check_history];
			double buy_cost_avg = array[0][check_length * check_history];

			while (check_since > check_length * (check_history-1)) {
				double to_buy, to_buy_t;
				double to_buy_cost, to_buy_cost_t;

				check_since -= check_interval;

				//to_buy = answer->to_buy*100;
				to_buy_t = to_buy = get_to_buy(&array[0][check_since], array_len[0] - check_since, check_interval);
				printf("%lf to_buy: %lf\t", array[0][check_since], to_buy);

				if (fabs(to_buy) < (0.005 / array[0][check_since] / check_interval))
					to_buy_t = to_buy = 0;
				if (array[0][check_since] > buy_cost_avg*require_result_sell && value > 0.0000001) {
					to_buy = -0.1;
				}
				if (array[0][check_since] > buy_cost_avg_t*require_result_sell && value_t > 0.0000001) {
					to_buy_t = -0.1;
				}


				printf("to_buy: %lf\n", to_buy);
/*
				if (fabs(to_buy) / array[0][check_since] < (require_result-1))
					to_buy = 0;
*/
	//			to_buy /= 3000000;

				if (-to_buy > value)
					to_buy = -value;

				to_buy_cost = to_buy * array[0][check_since];


				if (to_buy_cost > money)
					to_buy_cost = money;

				to_buy = to_buy_cost / array[0][check_since];

				if (fabs(to_buy) < 0.000000001) {
					to_buy = 0;
					to_buy_cost = 0;
				}

				if (to_buy > 0) {
					buy_cost_avg = (buy_cost_avg*value + to_buy_cost) / (value + to_buy);
				} else {
					if (array[0][check_since] < buy_cost_avg*require_result) {
						to_buy      = 0;
						to_buy_cost = 0;
					}
				}

				if (to_buy > 0)
					to_buy *= 0.997;
				else
					to_buy *= 1.003;

				//fprintf(stderr, "Check: %u %lf %lf %lf %lf;\t", check_since, array[0][check_since], money, value, buy_cost_avg);

				value += to_buy;
				money -= to_buy_cost;

				if (fabs(value) < 0.000001) {
					buy_cost_avg = 0;
				}
/*
				if (fabs(to_buy_t) / array[0][check_since] < (require_result-1))
					to_buy_t = 0;
*/
	//			to_buy_t /= 3000000;

				if (-to_buy_t > value_t)
					to_buy_t = -value_t;

				to_buy_cost_t = to_buy_t * array[0][check_since];

				if (to_buy_cost_t > money_t)
					to_buy_cost_t = money_t;

				to_buy_t = to_buy_cost_t / array[0][check_since];

				if (fabs(to_buy_t) < 0.000000001) {
					to_buy_t = 0;
					to_buy_cost_t = 0;
				}

				if (to_buy_t > 0) {
					buy_cost_avg_t = (buy_cost_avg_t*value_t + to_buy_cost_t) / (value_t + to_buy_t);
				} else {
					if (array[0][check_since] < buy_cost_avg_t*require_result) {
						to_buy_t      = 0;
						to_buy_cost_t = 0;
					}
				}

				if (to_buy_t > 0)
					to_buy_t *= 0.997;
				else
					to_buy_t *= 1.003;

				if (!blocked) {
					value_t += to_buy_t;
					money_t -= to_buy_cost_t;
				}

				if (fabs(value_t) < 0.000001) {
					buy_cost_avg_t = 0;
				}

				fprintf(stderr, "CheckF (%i): %lf %lf %lf %e %e\n", check_i, money_t + value_t * array[0][check_since], money_t, value_t, to_buy_t, to_buy_cost_t);

				check_i++;

			}

			double money_total = money + value*array[0][check_since];

			//fprintf(stderr, "CheckR: %lf %lf %lf %lf\n", money_total, money_t + value_t * array[0][check_since], buy_cost_avg_t, value_t);

			k_sum += money_total;

			blocked = 0;//money_total < 1.001;
		}

		fprintf(stderr, "CheckR-avg: %lf\n", k_sum/(check_history_orig-1));

	}

	double to_buy = get_to_buy(array[0], array_len[0], 10);
	printf("%0.16lf\n", to_buy);

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

