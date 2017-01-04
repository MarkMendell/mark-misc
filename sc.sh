#!/bin/sh
if test ! -p /tmp/scstreamctl; then
	mkfifo /tmp/scstreamctl
fi
scstream ~/.scstream <~/.sccookie 3</tmp/scstreamctl | scview 3>/tmp/scstreamctl 4>>~/scpicks
