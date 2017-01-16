#!/bin/sh
printf '\33[7l' >/dev/tty
lines=$(cat)
len=$(echo "$lines" | wc -l)
i=1
while true; do
	line=$(echo "$lines" | sed -n "$i p")
	printf '%s\r' "$line" >/dev/tty
	c=$(getch </dev/tty)
	printf '\33[K' >/dev/tty
	case "$c" in
		[A|[D )
			i=$((i - 1)) ;;
		[B|C )
			i=$((i + 1)) ;;
		 )
			exit ;;
		 )
			echo "$line"
			break ;;
	esac
	test $i -gt $len && i=$len
	test $i -lt 1 && i=1
done
printf '\33[7h' >/dev/tty
