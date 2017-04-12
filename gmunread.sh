#!/bin/sh
nthstring()
{
	printf "%s" "$1" | awk -v n=$2 '
	{
		escaped = 0
		instring = 0
		stringi = 0
		for (i=1; i<=length(); i++) {
			c = substr($0, i, 1)
			if (!instring)
				instring = c == "\""
			else if ((c == "\"") && !escaped) {
				instring = 0
				if (stringi == n)
					break
				stringi++
			} else if ((stringi == n) && ((c != "\\") || escaped))
				printf("%s", c)
			escaped = (c == "\\") && !escaped
		}
	}'
}


to=/tmp/gunread.to.$$
from=/tmp/gunread.from.$$
mkfifo $to $from && trap "rm $to $from" EXIT
tls imap.gmail.com 993 >$from <$to & exec 3<$from 4>$to
authstring=$(cat)
cat <<-EOF >&4 &
	1 AUTHENTICATE PLAIN
	$authstring
	1 SELECT INBOX
	1 SEARCH UNSEEN
	EOF
head -n 12 <&3 >/dev/null
ids=$(head -n 1 <&3 | tail -c +10 | tr -d  | tr ' ' ,)
test "$ids" || exit
echo "1 FETCH $ids ENVELOPE" >&4 &
idcount=$(echo "$ids" | tr -d [:digit:] | wc -c)
i=0
while test $i -lt $idcount; do
	read -r entry
	printf "$(echo "$entry" | cut -f 6-7 -d ' ')"
	subjstart=$(echo "%s" "$entry" | cut -f 3- -d '"')
	if echo "$subjstart" | grep '^ NIL' >/dev/null; then
		printf " %s\n" "$(nthstring "$entry" 1 )"
	else
		printf " %s [4m%s[0m\n" "$(nthstring "$entry" 2)" "$(nthstring "$entry" 1)"
	fi
	i=$((i + 1))
done <&3
rm $to $from
