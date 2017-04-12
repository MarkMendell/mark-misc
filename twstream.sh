#!/bin/sh
bearer=
if ! test "$bearer"; then
	printf %s "twstream: bearer must be set in twstream's source code (see " >&2
	echo 'https://dev.twitter.com/oauth/reference/post/oauth2/token)' >&2
	exit 1
fi

if test "$1" = -r; then
	shift
else
	repliesq="&exclude_replies=1"
fi
test -f "$2" && sinceq="&since_id=$(cat <"$2")"
userq="screen_name=$1"

odd=0
while true; do
	q="$userq$sinceq$repliesq${sinceq:+&count=200}"
	while read -r tweet; do
		# Exit if no tweets
		test "$tweet" || exit

		# Show tweet, alternating grey and normal
		test $odd -eq 1 && printf '[0;37m'  # every other is gray
		printf %s "$tweet" | jget text | sed 's/&amp;/\&/g' | sed 's/&gt;/>/g' |\
			sed 's/&lt;/</g' | jdecode
		printf '[0;0m'
		odd=$((1 - odd))

		# Handle commands
		id=$(printf %s "$tweet" | jget id)
		while true; do
			case "$(getch)" in
				o ) echo "twitter.com/$1/status/$id" | pbcopy ;;
				 ) exit ;;
				 ) break ;;
			esac
		done

		# Save as last tweet processed
		test "$2" && echo $id >"$2"
		sinceq="&since_id=$id"
	done <<-EOF1
		$(tls api.twitter.com <<-EOF2 | tail -n 1 | jvals | tac
			GET /1.1/statuses/user_timeline.json?$q HTTP/1.1
			Host: api.twitter.com
			Authorization: Bearer $bearer
			Connection: close
			
			EOF2
		)
		EOF1
done
