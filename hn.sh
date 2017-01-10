#!/bin/sh
asksync()
{
	printf 'Sync? [Y/n] '
	read answer
	test ! "$answer" = n
}


asksync && scp mmendell@unix.andrew.cmu.edu:streams/.hn* ~
if test ! -p /tmp/hnstreamctl; then
	mkfifo /tmp/hnstreamctl
fi
syncback() { asksync && scp ~/.hn* mmendell@unix.andrew.cmu.edu:streams; }
trap "trap - INT; syncback; exit" INT
hnstream ~/.hnstream </tmp/hnstreamctl | hnview 3>/tmp/hnstreamctl 4>>~/.hnyes 5>>~/.hnno
trap - INT
syncback
