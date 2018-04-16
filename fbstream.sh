#!/bin/sh
die() { echo "$1" >&2; exit 1; }
dechunk() { sed -n 'H;${g;s/\n[0-9a-f]\{1,\}\n//g;s/.*\n//p;}'; }

token=
test "$token" || die 'fbstream: token not set'
test "$1" || die 'usage: fbstream pageid [fbefore]'
test -f "$2" && since=$(cat "$2")
q="access_token=$token&date_format=U&fields=created_time,permalink_url,message"
while true; do
	qsince=${since:+&limit=100&since=$since}
	posts=$(<<-! tls graph.facebook.com 443 | dechunk | jget data | jvals
		GET /v2.11/$1/posts?$q$qsince HTTP/1.1
		host: graph.facebook.com
		connection: close
		
		!
	)
	test "$posts" || break
	while read -r post; do
		test "$url" && echo -----------------------
		printf %s "$post" | jget message | jdecode
		url=$(printf %s "$post" | jget permalink_url | jdecode)
		while true; do
			{
				stty -icanon -echo -icrnl
				c=$(dd bs=8 count=1 2>/dev/null)
				stty icanon echo icrnl
			} </dev/tty
			case "$c" in
				) break ;;
				o)
					chrome-cli open "$url" >/dev/null
					open /Applications/Google\ Chrome.app ;;
				*) echo ? >&2 ;;
			esac
		done
		test "$2" && printf %s "$post" | jget created_time >"$2"
	done <<-!
		$(printf '%s\n' "$posts" | awk '{l[NR]=$0}END{for(i=NR;i;i--)print l[i]}')
		!
	since=$(printf %s "$posts" | head -n 1 | jget created_time)
done
