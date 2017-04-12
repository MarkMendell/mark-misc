#!/bin/sh
case "$1" in
	1 ) mode=m ;;
	2 ) mode=j ;;
	* )
		echo "usage: mp3encode channels" >&2
		exit 1 ;;
esac

lame --silent -r -s 44.1 --little-endian -m $mode -V 0 - -
