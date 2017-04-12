#!/bin/sh
cd ~/.gmestream
for group in *; do
	gmestream $group $group
done
