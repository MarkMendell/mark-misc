#!/bin/sh
cd ~/.slkstream
for channel in *; do
	slkstream $channel $channel
done
