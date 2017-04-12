#!/bin/sh
slack()
{
	response=$(tls slack.com <<-EOF
			GET /api/${1}token=$token HTTP/1.1
			Host: slack.com
			Connection: close
			
			EOF
		)
	data=$(printf %s "$response" | sed '1,/^$/d')
	if printf %s "$response" | grep -i '^transfer-encoding: ch' >/dev/null; then
		printf %s "$data" | sed -n 'n;p'
	else
		printf %s "$data"
	fi | tr -d '\n\r'
}

idtoname()
{
	while read -r id; do
		printf %s "$members" | grep "^$id" | cut -f 2
	done
}


token=
if ! test "$token"; then
	printf "slkstream: token must be set in slkstream's source (see " >&2
	echo 'https://api.slack.com/docs/oauth-test-tokens)' >&2
	exit 1
fi

members=$(slack users.list? | jget members | jvals | while read -r member; do
		printf '%s\t' "$(printf %s "$member" | jget id)"
		printf '%s\n' "$(printf %s "$member" | jget name | jdecode)"
	done)

test -f "$2" && oldestq="&count=1000&oldest=$(cat <"$2")"
slack "channels.history?channel=$1$oldestq&" | jget messages | jvals | tac |\
		while read -r msg; do
	printf '[4m%s[0m ' "$(printf %s "$msg" | jget user | idtoname)"
	text=$(printf %s "$msg" | jget text | jdecode | sed 's/&gt;/>/g')
	while printf %s "$text" | grep '<@[^>]*>' >/dev/null; do
		tabtext=$(printf %s "$text" | paste -s -)
		id=$(printf %s "$tabtext" | sed 's/^.*<@\([^>]*\)>.*$/\1/g')
		escname=$(echo $id | idtoname | sed 's/[\/&]/\\&/g')
		text=$(printf %s "$tabtext" | sed "s/<@$id>/@$escname/g" | tr '\t' '\n')
	done
	printf '%s\n[0;37m' "$text"
	printf %s "$msg" | jget reactions 2>/dev/null | jvals |\
			while read -r reaction; do
		printf '[%s: ' "$(printf "$reaction" | jget name | tr _ ' ')"
		printf %s "$reaction" | jget users | jvals | idtoname | paste -s - |\
			sed 's/	/, /g; s/$/]/g'
	done
	printf [0;0m
	test "$(getch)" =  && exit
	test "$2" && printf %s "$msg" | jget ts >"$2"
done
