#!/bin/sh
if test ! -p /tmp/fromhn; then
	mkfifo /tmp/fromhn /tmp/tohn /tmp/hnstreamctl
	tls -r hacker-news.firebaseio.com >/tmp/fromhn </tmp/tohn &
fi
hnstream ~/.hnstream 3</tmp/fromhn 4>/tmp/tohn 5</tmp/hnstreamctl | hnview 3>/tmp/hnstreamctl
