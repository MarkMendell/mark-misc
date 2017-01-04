#!/bin/sh
scget()
{
	tls api.soundcloud.com <<-EOF 2>/dev/null | tail -n 1
		GET $1?client_id=fDoItMDbsbZz8dY16ZzARCZmzgHBPotA HTTP/1.1
		host: api.soundcloud.com
		Connection: close
		
		EOF
}

getch()
{
	{
		stty raw;
		dd bs=3 count=1 2>/dev/null;
		stty -raw;
	} </dev/tty
}


if test ! -p /tmp/scviewmpvctl; then
	mkfifo /tmp/scviewmpvctl
fi
while read posttype b c d; do
	if test $posttype = r; then
		printf 'Repost by %s:\n' "$(scget /users/$b | jget username | jdecode)"
		type=$c
		id=$d
	else
		type=$b
		id=$c
	fi
	if test $type = t; then
		info=$(scget /tracks/$id)
	else
		info=$(scget /playlists/$id)
	fi
	printf '%s - \33[4m%s\33[0m' "$(echo "$info" | jget user username)" "$(echo "$info" | jget title)"
	if test $type = p; then
		printf ' (%s tracks)' "$(echo "$info" | jget tracks | jvals | wc -l)"
	fi
	printf '\n'
	while true; do
		case "$(getch)" in
			d )
				echo "$info" | jget description | jdecode ;;
			p )
				url=$(echo "$info" | jget permalink_url | jdecode)
				mpv --no-audio-display --input-file /tmp/scviewmpvctl "$url" &
				while true; do
					case "$(getch)" in
						[C )
							echo seek 5 ;;
						[D )
							echo seek -5 ;;
						 )
							echo quit
							break ;;
					esac
				done >/tmp/scviewmpvctl
				wait
				printf '\r' ;;
			s )
				echo $type $id >&4 ;;
			 )
				exit ;;
			 )
				break ;;
		esac
	done
	echo >&3
done
