#!/bin/sh
# Find a line in $1 that looks like 'var $2 = <...>;' and assign $2 to the value of evaluating the
# ... stuff arithmetically.
readvar()
{
	value=$(($(echo "$1" | grep var\ $2 | sed "1 s/.*var $2 = \(.*\);.*/\1/g; q")))
	eval "$2=$value"
}


if test "$1" = -s; then
	save=1
	shift
fi
dlurl=$(echo "$1" | sed 's/\.com.*/.com/g')
page=$(curl -s "$1")
dlurl=$dlurl$(echo "$page" | grep \'dlbutton\' | cut -f 2 -d \")
readvar "$page" m
readvar "$page" k
readvar "$page" z
readvar "$page" x
dlurl=$dlurl$(($(echo "$page" | grep \'dlbutton\' | cut -f 2-5 -d +)))
dlurl=$dlurl$(echo "$page" | grep \'dlbutton\' | cut -f 4 -d \")
if test $save; then
	curl -s "$dlurl" > "$(basename "$dlurl" | urldecode)"
else
	curl -s "$dlurl"
fi
