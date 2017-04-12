#!/bin/sh
cd ~/.twstream
for args in *; do
	name=${args#-r }
	twstream $args "$args" | {
		read -r line || continue
		printf '\33[4m%s\33[0m\n' $name
		printf '%s\n' "$line"
		cat
	}
done
