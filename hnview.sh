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
	unset readitem
	while true; do
		case "$({ stty raw -echo; dd bs=1 count=1 2>/dev/null; stty -raw echo; } </dev/tty)" in
			o )
				readitem=1
				echo "$info" | jget url | jdecode | pbcopy ;;
			O )
				readitem=1
				echo https://news.ycombinator.com/item?id=$id | pbcopy ;;
			 )
				exit ;;
			 )
				break ;;
		esac
	done
	if test $readitem; then
		echo $id >&4
	else
		echo $id >&5
	fi
	echo >&3
done
