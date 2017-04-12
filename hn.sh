#!/bin/sh
asksync()
{
	printf 'Sync? [Y/n] '
	read answer
	test ! "$answer" = n
}


asksync && scp mmendell@unix.andrew.cmu.edu:streams/.hnstream ~
syncback() { asksync && scp ~/.hnstream mmendell@unix.andrew.cmu.edu:streams; }
trap "trap - INT; syncback; exit" INT
hnstream ~/.hnstream
trap - INT
syncback
