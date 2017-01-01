#!/bin/sh
if test "$1" = -r; then
	reconnect=1
	shift
fi
while true; do
	echo hi >&2
	cat >&2
	ready
	echo hey >&2
	openssl s_client -quiet -connect $1:${2:-443}
	if test ! $reconnect; then
		break
	fi
done
