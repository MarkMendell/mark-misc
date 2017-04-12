#!/bin/sh
html=$(tls www.messenger.com <<-EOF | grep '"participants":'
	GET / HTTP/1.1
	Host: www.messenger.com
	Connection: close
	Cookie: $(cat)
	User-Agent: tls.sh
	
	EOF
	)
yourid=$(printf %s "$html" | sed 's/^.*"ORIGINAL_USER_ID":"\([0-9]*\)".*$/\1/g')
data=$(printf %s "$html" | sed 's/^.*"mercuryPayload"://g')
while read -r user; do
	id=$(printf %s "$user" | jget id)
	test ${id#fbid:} = $yourid && continue
	name=$(printf %s "$user" | jget name | jdecode)
	users=$(printf "%s${users:+\n}%s\t%s" "$users" $id "$name")
done <<-EOF
	$(printf %s "$data" | jget participants | jvals)
	EOF
printf %s "$data" | jget threads | jvals | sed '$d' | tac |\
		while read -r chat; do
	unread=$(printf %s "$chat" | jget unread_count)
	test $unread -gt 0 || continue
	name=$(printf %s "$chat" | jget name | jdecode)
	test "$name" || name=$(printf %s "$chat" | jget participants | jvals |\
		while read id; do
			printf %s "$users" | grep "^$id" | cut -f 2
		done | paste -s - | sed 's/	/, /g')
	printf '(%d) %s\n' $unread "$name"
	printf '[0;37m%s[0;0m\n' "$(printf %s "$chat" | jget snippet | jdecode)"
done
