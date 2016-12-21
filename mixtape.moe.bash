#!/bin/bash
# Vastly stripped down from https://gitgud.io/snippets/34
if [ $# -ne 1 ]; then
	echo "usage: mixtape.moe file" >&2
	exit 1
fi

output=$(curl --silent -sf -F files[]="@$1" "https://mixtape.moe/upload.php")
if [[ "${output}" =~ '"success":true,' ]]; then
	echo "$(echo "$output" | grep -Eo '"url":"[A-Za-z0-9]+.*",' | sed 's/"url":"//;s/",//' | sed 's/\\//g')"
else
	echo "mixtape.moe: failed to upload" >&2
	exit 1
fi
