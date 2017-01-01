#!/bin/sh
# Set $1 to the next character input to the terminal.
getch()
{
	stty raw
	eval "$1=$(dd bs=1 count=1 2>/dev/null)"
	stty -raw
}


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
		getch c </dev/tty
		if test "$c" = o; then
			echo "$info" | jget url | jdecode | pbcopy
		elif test "$c" = O; then
			echo "https://news.ycombinator.com/item?id=$id" | pbcopy
		elif test "$c" = ; then
			exit
		else
			break
		fi
	done
	echo >&3
done
