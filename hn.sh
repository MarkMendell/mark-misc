#!/bin/sh
if test ! -p /tmp/hnstreamctl; then
	mkfifo /tmp/hnstreamctl
fi
hnstream ~/.hnstream 3</tmp/hnstreamctl | hnview 3>/tmp/hnstreamctl
