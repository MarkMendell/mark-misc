#!/bin/sh
mbget()
{
	tcp musicbrainz.org <<-EOF | sed '1,/^$/d' | while read -r line; do
			GET $1 HTTP/1.1
			Host: musicbrainz.org
			User-Agent: Mozilla/5.0
			Connection: close
			
			EOF
		test $2 && echo "$line"
		read -r line && echo "$line"
	done | tr -d '\n\r'
}


printf 'Artist: ' && read artist
printf 'Album: ' && read album
while read entry; do
	albumartist=$(echo "$entry" | jget artist-credit 0 artist name | jdecode)
	album=$(echo "$entry" | jget title | jdecode)
	id=$(echo "$entry" | jget id)
	year=$(echo "$entry" | jget date | cut -f 1 -d -)
	echo "$albumartist"
	echo "$album"
	echo "$entry" | jget score; echo
	printf 'Use this? [y/N] ' && read answer </dev/tty
	test "$answer" = y && break
done <<-EOF
	$(mbget "/ws/2/release?fmt=json&query=artist:\"$(urlencode "$artist")\"+AND+release:\"$(urlencode "$album")\"" |\
		jget releases | jvals)
	EOF
test "$answer" = y || exit
tracks=$(mbget "/ws/2/release/$id?inc=recordings+artist-credits&fmt=json" true | jget media 0 tracks | jvals)

cd "$1"
for file in *.flac; do
	filenum=$(echo "$file" | cut -f 1 -d ' ' | cut -f 1 -d .)
	unset matched
	while read track; do
		tracknum=$(echo "$track" | jget number)
		if test $tracknum -eq "$filenum"; then
			matched=1
			unset artist
			while read credit; do
				artist=$artist$(echo "$credit" | jget name)$(echo "$credit" | jget joinphrase)
			done <<-EOF
				$(echo "$track" | jget artist-credit | jvals)
				EOF
			title=$(echo "$track" | jget title)
			metaflac --remove-all-tags\
				--set-tag "ALBUM=$album"\
				--set-tag "ALBUMARTIST=$albumartist"\
				--set-tag "ARTIST=$artist"\
				--set-tag "DATE=$year"\
				--set-tag "TITLE=$title"\
				--set-tag "TRACKNUMBER=$tracknum"\
				"$file"
			mv "$file" "$(printf "%02d %s.flac" "$tracknum" "$title")"
		fi
	done <<-EOF
		$tracks
		EOF
	if test ! $matched; then
		printf 'No match for track %s\n.' "$filenum" >&2
		exit 1
	fi
done

printf 'Cover? [Y/n] ' && read answer
if test "$answer" != n; then
	uri=$(tcp <<-EOF coverartarchive.org | grep ^Location: | tail -c +30 | tr -d '\r'
			GET /release/$id/front HTTP/1.1
			Host: coverartarchive.org
			Connection: close
			
			EOF
		)
	if test ! "$uri"; then
		echo Cover not found. >&2
		exit 1
	fi
	url=$(tls <<-EOF archive.org | grep ^Location: | tail -c +19 | tr -d '\r'
			GET $uri HTTP/1.1
			Host: archive.org
			Connection: close
			
			EOF
		)
	host=$(echo "$url" | cut -f 1 -d /)
	tls <<-EOF $host | tail +13 >cover.jpg
		GET /$(echo "$url" | cut -f 2- -d /) HTTP/1.1
		Host: $host
		Connection: close
		
		EOF
fi
