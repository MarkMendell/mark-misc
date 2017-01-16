#!/bin/sh
nthstring()
{
	printf "%s" "$1" | awk -F '' -v n=$2 '
	{
		escaped = 0
		instring = 0
		stringi = 0
		for (i=0; i<NF; i++) {
			if (!instring)
				instring = $i == "\""
			else if (($i == "\"") && !escaped) {
				instring = 0
				if (stringi == n)
					break
				stringi++
			} else if ((stringi == n) && (($i != "\\") || escaped))
				printf("%s", $i)
			escaped = ($i == "\\") && !escaped
		}
	}'
}


to=/tmp/gmview.to.$$
from=/tmp/gmview.from.$$
mkfifo $to $from
tls imap.gmail.com 993 >$from <$to & exec 3<$from 4>$to
authstring=$(cat)
cat <<-EOF >&4 &
	1 AUTHENTICATE PLAIN
	$authstring
	1 SELECT INBOX
	1 SEARCH UNSEEN
	EOF
head -n 12 <&3 >/dev/null
for id in $(head -n 1 <&3 | tail -c +10 | tr -d ); do
	echo "1 FETCH $id ENVELOPE" >&4 &
	read -r entry <&3
	printf "$(echo "$entry" | cut -f 6-7 -d ' ')"
	subjstart=$(echo "%s" "$entry" | cut -f 3- -d '"')
	if echo "$subjstart" | grep '^ NIL' >/dev/null; then
		printf " %s\n" "$(nthstring "$entry" 1)"
	else
		printf " %s \33[4m%s\33[0m\n" "$(nthstring "$entry" 2)" "$(nthstring "$entry" 1)"
	fi
	while true; do
		case "$(getch)" in
			o )
				echo "1 FETCH $id BODY" >&4 &
				head -n 1 <&3 >/tmp/gmview.html
				open /tmp/gmview.html ;;
			r )
				${EDITOR:-vi} /tmp/gmview.reply.$id
				reply=$(cat /tmp/gmview.reply.$id)
				if test "$reply"; then
					printf "1 REPLY $id %s\n" >&4
				fi
				head -n 1 <&3 >/dev/null ;;
			u )
				echo "1 SETFLAGS $id UNREAD" >&4
				head -n 1 <&3 >/dev/null ;;
			' ' ) 
				echo "1 FETCH $id BODY" >&4 &
				head -n 1 <&3 ;;
			 )
				exit ;;
			 )
				break ;;
		esac
	done
done
rm $to $from
