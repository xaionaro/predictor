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

#include "configuration.h"
#include "macros.h"

#include <stdlib.h>	/* size_t	*/
#include <string.h>	/* memset()	*/

#include "error.h"
#include "malloc.h"
#include "predictor.h"

#define M 3

static inline double approxfunct(double a, double b, double c, double x)
{
	debug(5, "a == %lf, b == %lf, c == %.10lf, x == %lf  -->  %lf", a, b, c, x, a*x*x + b*x + c);
	return a*x*x + b*x + c;
}

static inline void getsums(double sum[M], size_t start, size_t center, size_t end, double *array)
{
	size_t i;
	memset(sum, 0, sizeof(double)*M);

	i = start;
	while (i <= end) {
		double k;
		int m;

		k = 1;
		m = 0;
		while (m < M) {
			debug(6, "sum[%i] (%lf) += %lf * %lf", m, sum[m], k, array[i]);
			sum[m] += k*array[i];
			k      *= (double)i-center;
			debug(6, "sum[%i] == %lf; k == %lf (%li, %li)", m, sum[m], k, i, center);
			m++;
		}

		i++;
	}

	return;
}

static inline void getcoeffs(double c[M], double sum[M], double k[M])
{
	int    m = 0;
	while (m < M) {
		debug(5, "c[%i] = %lf * %lf", m, sum[m], k[m]);
		c[m] = sum[m] * k[m];
		m++;
	}

	return;
}
/*
static inline double findmax(double *array, size_t start, size_t end)
{
	size_t i;
	double max = array[start];

	i = start+1;
	while (i <= end) {
		max = MAX(max, array[i]);
		i++;
	}

	return max;
}

static inline double findmin(double *array, size_t start, size_t end)
{
	size_t i;
	double min = array[start];

	i = start+1;
	while (i <= end) {
		min = MIN(min, array[i]);
		i++;
	}

	return min;
}
*/
predanswer_t *predictor(size_t n, double *array)
{
	static predanswer_t answer;
	double *temparray = xmalloc(sizeof(double) * n);

	size_t end    = n-1;

	critical_on (end <= 0);
	size_t start  = MAX(0, (long)end - PREDICTOR_APPROXIMATE_LEN);

	critical_on (((start+end) % 2));

	size_t center = (start+end) / 2;

	debug(3, "n == %i, start == %i, center == %i, end == %i", n, start, center, end);

	// Calculating coefficients
	double c[M];
	size_t path_avg = end - center;
	{
		// Calculating sums
		double sum[M];
		getsums(sum, start, center, end, array);

		// And now calculating the coefficients
		{ 
			double k[M] = {0};

			// Calculating k[0]
			k[0] = (double)1 / (end+1-start);

			// Calculating k[1]
			{
				size_t i;
				double p;
				k[1] = -1;
				p    = -1;
				i    = 0;
				while (i <= end) {
					p    += 2;
					k[1] += p;
					i++;
				}
				debug(4, "p == %lf; k[1] == %lf", p, k[1]);
				k[1] = 12*k[0]/k[1];
				debug(4, "k[1] == %lf", k[1]);
			}

			// And at last calculating the goddamn coefficients (except c[2])
			getcoeffs(c, sum, k);
			
			debug(3, "getting c[2]");
			// Calculating c[2]
			{
				double tempsum[M] = {0};
				double tempc[M];
				size_t i;
				i = start;
				while (i <= end) {
					double shift;
					shift = approxfunct(0, c[1], c[0], (double)i-center);
					temparray[i] = array[i] - shift;
					debug(8, "temparray[%li] = %lf - %lf -> %lf", i, array[i], shift, temparray[i]);
					i++;
				}
				getsums(tempsum, start, center, end, temparray);
				debug(3, "sign is %i", sum[2] >= 0 ? 1 : -1);

				double shift;
				if (sum[2] >= 0)
//					shift = -findmin(temparray, start, end);
					shift = -temparray[center];
				else
//					shift = -findmax(temparray, start, end);
					shift = temparray[center];
				debug(4, "shift == %lf", shift);

				i = start;
				while (i <= end) {
					temparray[i] += shift;
					debug(8, "temparray[%li] == %lf", i, temparray[i]);
					i++;
				}

				getsums(tempsum, start, center, end, temparray);

				k[2]  = 1 / (double)path_avg / (double)path_avg / (double)path_avg / (double)path_avg / (double)path_avg / 2;
				getcoeffs(tempc, tempsum, k);
				c[2]  = tempc[2];
//				c[0] -= shift;
//				c[0] += c[1]*path_avg;
			}

			// Normalizing c[0]
			{
				size_t i;
				double tempc1[M];//,   tempc2[M];
				double tempsum1[M];//, tempsum2[M];
				i = start;
				while (i <= end) {
					temparray[i] = approxfunct(c[2], c[1], c[0], (double)i-center);
					i++;
				}
				getsums(tempsum1, start, center, end, temparray);
				getcoeffs(tempc1, tempsum1, k);
//				getsums(tempsum2, start, center, end, array);
//				getcoeffs(tempc2, tempsum2, k);

				double cdiff = c[0] - tempc1[0];
				debug(3, "cdiff == %lf", cdiff);

				c[0] += cdiff;
			}
		}
	}

	debug(1, "%lf (%lf [%lf], %lf [%lf]); path_avg == %lf", c[0], c[1], c[0]*(path_avg/2), c[2], c[0]*(path_avg/2)*(path_avg/2), path_avg);

	// Calculating the coefficients
	{
		size_t i;
		i = start;
		while (i <= end) {
			i++;
		}
	}

	answer.approximated_currency	= approxfunct(c[2], c[1], c[0], path_avg);
	answer.to_buy			= (answer.approximated_currency - array[end]) / array[end];

	free(temparray);
	return &answer;
}

