le()
{
	test $1 -gt 511 && { echo $1 TOO BIG >&2; exit 1; }
	printf "\\$(($1/64))$((($1%64)/8))$(($1%8))"
	i=-1; while test $((i+=1)) -lt $(($2-1)); do
		printf "\0"
	done
}

t=-1
tag()
{
	le $((t+=1)) 2
}

f=-1
fid()
{
	le $((f+=1)) 4
}

size()
{
	len=$1
	shift
	for s; do
		len=$((len + 2 + $(printf %s "$s" | wc -c)))
	done
	le $len 4
}

version()
{
	size 11 9P2000
	printf '\144\377\377\0\3\0\0'
	str 9P2000
}

str()
{
	le $(printf %s "$1" | wc -c) 2
	printf %s "$1"
}

auth()
{
	size 11 "$1" "$2"
	printf '\146'
	tag
	fid
	str "$1"
	str "$2"
}

flush()
{
	size 9
	printf '\154'
	tag
	le $1 2
}

attach()
{
	size 15 "$2" "$3"
	printf '\150'
	tag
	fid
	le $1 4
	str "$2"
	str "$3"
}

version
auth enebo foo
attach 0 enebo foo
attach 0 enebo foo
attach 0 enebo foo
attach 0 enebo foo
attach 0 enebo foo
attach 0 enebo foo
attach 0 enebo foo
attach 0 enebo foo
attach 0 enebo foo
attach 0 enebo foo
attach 0 enebo foo
attach 0 enebo foo
attach 0 enebo foo
attach 0 enebo foo
attach 0 enebo foo
attach 0 enebo foo
attach 0 enebo foo
attach 0 enebo foo
attach 0 enebo foo
attach 0 enebo foo
attach 0 enebo foo
version
auth enebo foo
a=$((f-1))
attach $a enebo foo
attach $a enebo foo
attach $a enebo foo
attach $a enebo foo
attach $a enebo foo
auth enebo foo
auth enebo foo
auth enebo foo
auth enebo foo
attach $a enebo foo
attach $a enebo foo
attach $a enebo foo
attach $a enebo foo
attach $a enebo foo
attach $a enebo foo
attach $a enebo foo
attach $a enebo foo
attach $a enebo foo
attach $a enebo foo
attach $a enebo foo
attach $a enebo foo
attach $a enebo foo
attach $a enebo foo
attach $a enebo foo
sleep 1
