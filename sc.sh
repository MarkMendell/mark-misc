#!/bin/sh
scp mmendell@unix.andrew.cmu.edu:streams/.scstream ~
if test ! -p /tmp/scstreamctl; then
	mkfifo /tmp/scstreamctl
fi
syncback() { scp ~/.scstream mmendell@unix.andrew.cmu.edu:streams; }
trap "trap - INT; syncback; exit" INT
scstream ~/.scstream <~/.sccookie 3</tmp/scstreamctl | scview 3>/tmp/scstreamctl 4>>~/scpicks
trap - INT
syncback
