#!/bin/sh
getid() { sed -n '/ORIGINAL_USER_ID/s/.*ORIGINAL_USER_ID":"\([^"]*\).*/\1/p'; }
getchats()
{
	subchats='/"message_threads"/s/.*"message_threads":\(.*\)/\1/p'
	sed -n "$subchats" | jget nodes | jvals
}
getusers() { jget all_participants nodes | jvals | jget messaging_actor; }
response=$(<<-. tls www.messenger.com | sed -n 'H;${g;s/\n[0-9a-f]*\n//gp;}'
	GET / HTTP/1.1
	Host: www.messenger.com
	Connection: close
	Cookie: $(cat)
	User-Agent: tls.sh
	
	.
)
yourid=$(printf %s "$response" | getid)
printf %s "$response" | getchats | while read -r chat; do
	unread=$(printf %s "$chat" | jget unread_count)
	test $unread -gt 0 || continue
	name=$(printf %s "$chat" | jget name)
	test "$name" = null &&
		name=$(printf %s "$chat" | getusers | while read -r user; do
				test "$(printf %s "$user" | jget id)" = $yourid && continue
				printf %s "$user" | jget name
			done | paste -s - | sed 's/	/, /g' | jdecode)
	printf '(%d) %s\n' $unread "$name"
	msg=$(printf %s "$chat" | jget last_message nodes 0 snippet | jdecode)
	printf '[0;37m%s[0;0m\n' "$msg"
done
