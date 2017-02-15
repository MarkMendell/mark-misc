#!/bin/sh
while read id; do
	info=$(tls hacker-news.firebaseio.com <<-EOF | tail -n 1
		GET /v0/item/$id.json HTTP/1.1
		host: hacker-news.firebaseio.com
		Connection: close
		
		EOF)
	title=$(echo "$info" | jget title | jdecode)
	score=$(echo "$info" | jget score | jdecode)
	printf "%s %s\n" "$score" "$title"
	while true; do
		case "$({ stty raw -echo; dd bs=1 count=1 2>/dev/null; stty -raw echo; } </dev/tty)" in
			o )
				echo "$info" | jget url | jdecode | pbcopy ;;
			O )
				echo https://news.ycombinator.com/item?id=$id | pbcopy ;;
			 )
				exit ;;
			 )
				break ;;
		esac
	done
	echo >&3
done
