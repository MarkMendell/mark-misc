#!/bin/sh
if test -f "$1"; then
	prevuuid=$(cat "$1")
fi
token=$(cat)
while true; do
	unset entries offset wroteone
	while true; do
		entries=$(cat ~/stream-bak.json | tail -n 1 | jget collection | jvals | tac)
		#entries=$(tls api-v2.soundcloud.com <<-EOF 2>/dev/null | tail -n 1 | jget collection | jvals | tac
		#	GET /stream${offset:+?offset=}$offset HTTP/1.1
		#	host: api-v2.soundcloud.com
		#	Connection: close
		#	Authorization: OAuth $token
		#	
		#	EOF)$entries
		firstuuid=$(echo "$entries" | head -n 1 | jget uuid)
		if test ! $prevuuid || test $(strcmp $firstuuid $prevuuid) -le 0; then
			break;
		else
			offset=$firstuuid
		fi
	done
	while read -r entry; do
		uuid=$(echo "$entry" | jget uuid)
		if test ! $prevuuid || test $(strcmp $uuid $prevuuid) -gt 0; then
			#echo gettin type
			type=$(echo "$entry" | jget type)
			if test $type = track-repost || test $type = playlist-repost; then
				#echo gettin user id
				printf 'r %s ' $(echo "$entry" | jget user id)
			else
				printf 'p '
			fi
			if test $type = track || test $type = track-repost; then
				#echo gettin track id
				printf 't %s\n' $(echo "$entry" | jget track id)
			else
				#echo gettin playlist id
				printf 'p %s\n' $(echo "$entry" | jget playlist id)
			fi
			read <&3 || exit
			prevuuid=$uuid
			if test "$1"; then
				echo $prevuuid >"$1"
			fi
			wroteone=1
		fi
	done <<-EOF
		$entries
		EOF
	if test ! $wroteone; then
		sleep 7200
	fi
done
