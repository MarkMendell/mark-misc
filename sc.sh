#!/bin/sh
asksync()
{
	printf 'Sync? [Y/n] '
	read answer
	test ! "$answer" = n
}


asksync && scp mmendell@unix.andrew.cmu.edu:streams/.scstream ~
if test ! -p /tmp/scstreamctl; then
	mkfifo /tmp/scstreamctl
fi
syncback() { asksync && scp ~/.scstream mmendell@unix.andrew.cmu.edu:streams; }
trap "trap - INT; syncback; exit" INT
scstream ~/.scstream <~/.sccookie 3</tmp/scstreamctl | scview 3>/tmp/scstreamctl 4>>~/scpicks
trap - INT
syncback
