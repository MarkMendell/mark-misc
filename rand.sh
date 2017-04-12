#!/bin/sh
if [ $# -eq 1 ]; then
	from=0
	below=$1
elif [ $# -eq 2 ]; then
	from=$1
	below=$2
else
	echo "usage: rand below" >&2
	echo "       rand from below" >&2
	exit 1
fi
awk "BEGIN{srand(); print int($from + rand()*($below - $from))}"
