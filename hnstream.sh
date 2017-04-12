#!/bin/sh
hnget()
{
	tls hacker-news.firebaseio.com <<-EOF | tail -n 1
		GET $1 HTTP/1.1
		host: hacker-news.firebaseio.com
		Connection: close
		
		EOF
}


test -f "$1" && previd=$(cat <"$1")
ids=$(hnget /v0/topstories.json | jvals | sort -n)
for id in $ids; do
	(test $previd && test $id -le $previd) && continue
	wait=$(($(hnget /v0/item/$id.json | jget time) + 60*60*24 - $(timestamp)))
	test $wait -gt 0 && exit
	info=$(hnget /v0/item/$id.json)
	url=$(printf %s "$info" | jget url 2>/dev/null)
	title=$(printf %s "$info" | jget title | jdecode)
	score=$(printf %s "$info" | jget score | jdecode)
	printf '%s\n%s\n%s %s\n' $id "$url" $score "$title"
done | while read -r id && read -r url && read -r entry; do
	printf '%s\n' "$entry"
	while true; do
		case "$(getch)" in
			o ) 
				if test "$url"; then
					printf %s "$url" | pbcopy
				else
					echo '(no url)'
				fi ;;
			O ) printf %s https://news.ycombinator.com/item?id=$id | pbcopy ;;
			 ) exit ;;
			 ) break ;;
		esac
	done
	test "$1" && echo $id >"$1"
done
