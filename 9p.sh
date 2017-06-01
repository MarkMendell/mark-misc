t=0
tag()
{
	printf "\\$((t/8))$((t%8))\0"
	t=$((t+1))
}

f=0
fid()
{
	printf "\\$((f/8))$((f%8))\0\0\0"
	f=$((f+1))
}

version()
{
	printf '\23\0\0\0\144\377\377\0\3\0\0\6\0009P2000'
}

auth()
{
	printf '\27\0\0\0\146'
	tag
	fid
	printf '\5\0enebo\3\0foo'
}

flush()
{
	printf '\11\0\0\0\154'
	tag
	printf "\\$(($1/8))$(($1%8))\0"
}

version
auth
auth
auth
auth
auth
flush 4
cat
