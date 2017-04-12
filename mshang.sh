#!/bin/sh
cookie=$(cat)
to=/tmp/mshang.$$.to
from=/tmp/mshang.$$.from
mkfifo $to $from
trap "rm $to $from" EXIT

tls 4-edge-chat.messenger.com >$from <$to &

url="/pull?clientid=42$$&msgs_recv=0"
while true; do
	seq=$(printf %s "$data" | jget seq 2>/dev/null)
	cat <<-EOF
		GET $url$stickyq&seq=$seq HTTP/1.1
		Host: 4-edge-chat.messenger.com
		Cookie: $cookie
		
		EOF

	unset contentlength
	while read -r header && test "$header" != ; do
		echo "$header" | grep -i '^content-l' >/dev/null &&\
			contentlength=$(echo "$header" | cut -f 2 -d ' ' | tr -d )
	done

	if test $contentlength; then
		data=$(dd bs=1 count=$contentlength 2>/dev/null)
	else
		data=$(while read -r size && test "$size" != 0; do
				read -r line && printf %s "$line"
			done)
		read _
	fi
	data=$(printf %s "$data" | tr -d  | tail -c +11)  # 'for (;;); '

	t=$(printf %s "$data" | jget t)
	case "$t" in
		lb )
			stickytoken=$(printf %s "$data" | jget lb_info sticky)
			stickypool=$(printf %s "$data" | jget lb_info pool)
			stickyq="&sticky_pool=$stickypool&sticky_token=$stickytoken" ;;
		msg ) printf %s "$data" | grep '"class":"NewMessage"' >/dev/null && break ;;
	esac
done <$from >$to
