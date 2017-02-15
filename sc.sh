#!/bin/sh
asksync()
{
	printf 'Sync? [Y/n] '
	read answer
	test ! "$answer" = n
}


asksync && scp mmendell@unix.andrew.cmu.edu:streams/{.scstream,scpicks} ~
syncback() { asksync && scp ~/{.scstream,scpicks} mmendell@unix.andrew.cmu.edu:streams; }
trap "trap - INT; syncback; exit" INT
scstream ~/.scstream <~/.sccookie 3>>~/scpicks
trap - INT
syncback
