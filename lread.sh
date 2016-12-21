#!/bin/sh
while read -r line; do
	printf "%s" "$line"
	read </dev/tty
done
