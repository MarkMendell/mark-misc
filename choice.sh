#!/bin/sh
while getopts iv opt; do
	case $opt in
		i )
			wantindex=1 ;;
		v )
			wantvalue=1 ;;
	esac
done
if test $wantindex && test $wantvalue; then
	echo "choice: can't use i and v flags together" >&2
	exit 1
fi

# Get lines and values
lines=$(cat)
if test $wantvalue; then
	values=$(printf %s "$lines" | sed -n 'n;p')
	lines=$(printf %s "$lines" | sed -n 'p;n')
fi
len=$(printf '%s\n' "$lines" | wc -l)

# Let user choose line
printf [7l >/dev/tty
i=1
while true; do
	line=$(printf %s "$lines" | sed -n "$i p")
	printf '%s\r' "$line" >/dev/tty
	c=$(getch)
	printf [K >/dev/tty
	case "$c" in
		[A )
			i=$((i - 1)) ;;
		[B )
			i=$((i + 1)) ;;
		 )
			exit 1 ;;
		 )
			if test $wantindex; then
				echo $i
			elif test $wantvalue; then
				printf %s "$values" | sed -n "$i p"
			else
				printf %s "$line"
			fi
			break ;;
	esac
	test $i -gt $len && i=$len
	test $i -lt 1 && i=1
done
printf [7h >/dev/tty
