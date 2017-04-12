#!/bin/sh
groupme()
{
	response=$(tls api.groupme.com <<-EOF
			$1 /v3/${2}token=$token HTTP/1.1
			Host: api.groupme.com
			Connection: close
			
			EOF
		)
	data=$(printf %s "$response" | sed '1,/^$/d')
	if printf %s "$response" | grep '^transfer-encoding: chunked' >/dev/null; then
		printf %s "$data" | sed -n 'n;p'
	else
		printf %s "$data"
	fi | tr -d '\n\r' | jget response
}


token=
if ! test "$token"; then
	printf %s "gmeunread: token must be set in gemunread's source code (see " >&2
	echo 'https://dev.groupme.com/applications/new)' >&2
	exit 1
fi

while read -r member; do 
	id=$(printf %s "$member" | jget user_id)
	name=$(printf %s "$member" | jget nickname | jdecode)
	members=$(printf "%s${members:+\n}%s\t%s" "$members" $id "$name")
done <<-EOF
	$(groupme GET groups/$1? | jget members | jvals)
	EOF

while true; do
	test -f "$2" && q="limit=100&after_id=$(cat <"$2")&"
	unset read1
	groupme GET groups/$1/messages?$q | jget messages | jvals |\
			while read -r msg; do
		read1=1
		printf '[4m%s[0m ' "$(printf %s "$msg" | jget name | jdecode)"
		if ! printf %s "$msg" | grep '"text":null' >/dev/null; then
			printf %s "$msg" | jget text | jdecode
		fi
		printf '[0;37m'
		printf %s "$msg" | jget attachments | jvals | while read -r thing; do
			type=$(printf %s "$thing" | jget type)
			test "$type" = mentions && continue
			if test "$type" = image; then
				printf '[%s: %s]\n' "$type" "$(printf %s "$thing" | jget url)"
			else
				printf '%s\n' "$thing"
			fi
		done
		likes=$(printf %s "$msg" | jget favorited_by | jvals | while read -r id; do
			printf %s "$members" | grep "^$id" | cut -f 2
		done | paste -s - | sed 's/	/, /g')
		test "$likes" && printf '[💖  %s]\n' "$likes"
		printf '[0;0m'
		case "$(getch)" in
			l)
				id=$(printf %s "$msg" | jget id)
				groupme POST messages/$1/$id/like? >/dev/null
				echo 💖 ;;
			) exit ;;
		esac
		test "$2" && echo $(printf %s "$msg" | jget id) >"$2"
	done
	test $read1 || break
done
