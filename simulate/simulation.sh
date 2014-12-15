#!/bin/bash -e
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

WC=/usr/bin/wc
AWK=/usr/bin/awk
TAC=/usr/bin/tac
GREP=/bin/grep
HEAD=/usr/bin/head
TAIL=/usr/bin/tail
SLEEP=/bin/sleep

make -C ..

PATH="$2"

IHAVE_M_INITIAL=1000000


IHAVE_M=$IHAVE_M_INITIAL
IHAVE_S=0
ISPENT_MS=0

LINES="$($WC -l "$PATH" | $AWK '{print $1}')"

INITIAL_DAY="$1"
BOUGHT_AVG=9999999999

i=$INITIAL_DAY
while [[ $i -le $LINES ]]; do
	BUY_AMOUNT=$($TAC "$PATH" | $AWK -F ',' '{print $2}' | $GREP -v ^0 | $HEAD -"$i" | ../predictor | /usr/bin/tee /dev/stderr | $AWK -F '.' '{print $1}')
	CURRENCY=$($TAC "$PATH" | $AWK -F ',' '{print $2}' | $GREP -v ^0 | $HEAD -"$i" | $TAIL -1 | /usr/bin/tr -d '.')

#	BUY_AMOUNT=100

	echo "D: $i	DI: "$[ $i - $INITIAL_DAY ]"	RB: $BUY_AMOUNT;	CUR: $CURRENCY"
	i=$[ $i + 1 ]

	if [[ "$CURRENCY" = 'Open' ]]; then
		exit 0
	fi

#	BUY_AMOUNT=$[ $BUY_AMOUNT * -1 ]
	if [[ $BUY_AMOUNT -ge 0 ]]; then
		if [[ $[ $BUY_AMOUNT * $CURRENCY ] -ge $IHAVE_M ]]; then
			BUY_AMOUNT=$[ $IHAVE_M / $CURRENCY ]
#			$SLEEP 5
		fi
	else
		if [[ $BUY_AMOUNT -le -$IHAVE_S ]]; then
			BUY_AMOUNT=-$IHAVE_S
		fi
	fi

#	if [[ $BUY_AMOUNT -lt '0' ]]; then
#		if [[ $CURRENCY -le $BOUGHT_AVG ]]; then
#			echo "BOUGHT_AVG ($BOUGHT_AVG) is greater than CURRENCY ($CURRENCY). Skip."
#			#BUY_AMOUNT=$[ $BUY_AMOUNT / 2 ]
#			#$SLEEP 1
#			continue;
#		fi
#	fi

	if [[ $[$IHAVE_S + $BUY_AMOUNT] -eq '0' ]]; then
		BOUGHT_AVG=9999999999
	else
		if [[ "$BUY_AMOUNT" -gt '0' ]]; then
			BOUGHT_AVG=$[ ($BOUGHT_AVG*$IHAVE_S + $BUY_AMOUNT*$CURRENCY) / ($IHAVE_S + $BUY_AMOUNT) ]
		fi
	fi

	IHAVE_S=$[ $IHAVE_S + $BUY_AMOUNT ]
	IHAVE_M=$[ $IHAVE_M - $BUY_AMOUNT*$CURRENCY ]

	BHAVE_M=$[ $BHAVE_M + ${BUY_AMOUNT#-}*$CURRENCY / 100 ]
#	IHAVE_M=$[ $IHAVE_M - $BHAVE_M ]

	IHAVE_T=$[ $IHAVE_M + $IHAVE_S*$CURRENCY ]
	echo "D: $i	DI: "$[ $i - $INITIAL_DAY ]"	RB: $BUY_AMOUNT;	CUR: $CURRENCY	B: $BUY_AMOUNT	BA: $BOUGHT_AVG	M: $IHAVE_M	S: $IHAVE_S	T: $IHAVE_T	TAX: $BHAVE_M"
#	if [[ "$BUY_AMOUNT" -ne '0' ]]; then
#		$SLEEP 5
#	fi


#	if [[ $IHAVE_T -gt $[ $IHAVE_M_INITIAL * 200 / 100 ] ]]; then
#		echo Days: $[ $i - $INITIAL_DAY ] $i
#		exit 0
#	fi
#	if [[ $IHAVE_T -lt $[ $IHAVE_M_INITIAL * 70 / 100 ] ]]; then
#		echo Days: $[ $i - $INITIAL_DAY ] $i
#		exit 0
#	fi

	if [ $IHAVE_S -lt 0 -o $IHAVE_M -lt 0 ]; then
		exit -1
	fi

	#$SLEEP 1
done

exit 0
