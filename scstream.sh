#!/bin/sh
entries=$(tls api-v2.soundcloud.com <<-EOF 2>/dev/null | tail -n 1 | jget collection | jvals | tac
	GET /stream HTTP/1.1
	host: api-v2.soundcloud.com
	Connection: close
	Authorization: OAuth $(cat)
	
	EOF)
if test -f "$1"; then
	prevuuid=$(cat "$1")
else
	prevuuid=z
fi
printf "%s" "$entries" | while read -r entry; do
	echo "$entry"
done
