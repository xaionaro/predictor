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

#define M 3

struct predictor_answer {
	double	to_buy;
	double	sqdiff;
	double	approximated_currency;
	double	c[M];
};
typedef struct predictor_answer predanswer_t;

extern predanswer_t *predictor(size_t n, double *array);

