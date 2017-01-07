#!/bin/sh
hnget()
{
	tls hacker-news.firebaseio.com <<-EOF | tail -n 1
		GET $1 HTTP/1.1
		host: hacker-news.firebaseio.com
		Connection: close
		
		EOF
}


if test -f "$1"; then
	previd=$(cat "$1")
fi
ids=$(hnget /v0/topstories.json | jvals | sort -n)
for id in $ids; do
	if test $previd && test $id -le $previd; then continue; fi
	wait=$(($(hnget /v0/item/$id.json | jget time) + 60*60*24 - $(timestamp)))
	if test $wait -gt 0; then
		sleep $wait
	fi
	echo $id
	read _ <&3 || exit  # >tfw no unbuffered output
	if test "$1"; then
		printf $id >"$1"
	fi
done
