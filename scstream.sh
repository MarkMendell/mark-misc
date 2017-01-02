#!/bin/sh
if test -f "$1"; then
	prevuuid=$(cat "$1")
fi
token=$(cat)
while true; do
	unset entries
	while test $prevuuid && (test ! $firstuuid || test $(strcmp $firstuuid $prevuuid) -gt 0); do
		entries="$(tls api-v2.soundcloud.com <<-EOF 2>/dev/null | tail -n 1 | jget collection | jvals | tac
			GET /stream${firstuuid:+?offset=}$firstuuid HTTP/1.1
			host: api-v2.soundcloud.com
			Connection: close
			Authorization: OAuth $token
			
			EOF)$entries"
		firstuuid=$(echo "$entries" | head -n 1 | jget uuid)
	done
	unset wroteone
	while read -r entry; do
		uuid=$(echo "$entry" | jget uuid)
		if test ! $prevuuid || test $uuid -gt $prevuuid; then
			# output info
			prevuuid=$uuid
			wroteone=1
		fi
	done <<-EOF
		$entries
		EOF
	if test ! $wroteone; then
		sleep 7200
	fi
done
