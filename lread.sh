#!/bin/sh
while read -r line; do
	printf "%s" "$line"
	read _ </dev/tty
done
