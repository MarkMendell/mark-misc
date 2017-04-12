#!/bin/sh
APIKEY=AIzaSyAH8qmkkbPF-GHr9tgddxsHSdata4115oE
CSEID=005520904604546756045:o_zdpuw64au


tls www.googleapis.com <<-EOF | sed '1,/^$/d' | jget items | jvals | while read result; do
		GET /customsearch/v1?key=$APIKEY&cx=$CSEID&q=$(urlencode "$1")&prettyPrint=false HTTP/1.1
		Host: www.googleapis.com
		Connection: close
		
		EOF
	printf '%s: ' "$(printf %s "$result" | jget displayLink | tr -d '\n' | jdecode)"
	printf %s "$result" | jget title | jdecode
	printf %s "$result" | jget link
done | choice -v
