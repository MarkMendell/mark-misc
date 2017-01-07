#!/bin/sh
scp mmendell@unix.andrew.cmu.edu:streams/.hn* ~
if test ! -p /tmp/hnstreamctl; then
	mkfifo /tmp/hnstreamctl
fi
syncback() { scp ~/.hn* mmendell@unix.andrew.cmu.edu:streams; }
trap "trap - INT; syncback; exit" INT
hnstream ~/.hnstream 3</tmp/hnstreamctl | hnview 3>/tmp/hnstreamctl
trap - INT
syncback
