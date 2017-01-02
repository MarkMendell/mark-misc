#!/bin/sh
if test -f "$1"; then
	prevuuid=$(cat "$1")
fi
while true; do
	tls api-v2.soundcloud.com <<-EOF 2>/dev/null | tail -n 1 |\
				jget collection | jvals | tac | while read entry; do
			GET /stream${offset:+?offset=}$offset HTTP/1.1
			host: api-v2.soundcloud.com
			Connection: close
			Authorization: OAuth $(cat)
			
			EOF
		uuid=$(echo "$entry" | jget uuid)
		# Haven't found the last entry we output
		if test $prevuuid && test ! $whereweleftoff; then
			if test $(strcmp $uuid $prevuuid) -gt 0; then
				offset=$uuid
				continue 2
			else
				whereweleftoff=1
				unset offset
			fi
		fi
	done
	prevuuid=$uuid
	unset whereweleftoff
done
