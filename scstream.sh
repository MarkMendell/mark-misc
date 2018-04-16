#!/bin/sh
cmd()
{
	case "$1" in
		i )
			printf %s "$info" | jget description | jdecode
			echo ;;
		s )
			echo ${type%-repost} $(printf %s "$info" | jget id) >&3
			echo saved ;;
	esac
}

playurl()
{
	mpv --no-video --no-audio-display --input-file /tmp/scstream.$$.mpvctl "$1" &
	mpvpid=$!
	while true; do
		c=$(getch)
		# mpv exited, so this input is for the main commands
		if ! kill -0 $mpvpid 2>/dev/null; then
			savec=1
			break
		fi
		case "$c" in
			d )
				youtube-dl -x "$1" ;;
			o )
				printf %s "$1" | pbcopy ;;
			' ' )
				echo cycle pause >/tmp/scstream.$$.mpvctl ;;
			[A )
				echo seek 60 >/tmp/scstream.$$.mpvctl ;;
			[B )
				echo seek -60 >/tmp/scstream.$$.mpvctl ;;
			[C )
				echo seek 5 >/tmp/scstream.$$.mpvctl ;;
			[D )
				echo seek -5 >/tmp/scstream.$$.mpvctl ;;
			 )
				echo quit >/tmp/scstream.$$.mpvctl
				break ;;
			 )
				echo quit >/tmp/scstream.$$.mpvctl
				break ${2:-2} ;;
			* )
				cmd "$c" ;;
		esac
	done
	wait
	printf '\r'
}


mkfifo /tmp/scstream.$$.mpvctl
trap 'rm /tmp/scstream.$$.*' EXIT
token=$(cat)
test -f "$1" && prevuuid=$(cat <"$1")
while true; do
	unset entries offset wroteone
	# Get stream entries from soundcloud
	while true; do
		entries=$(tail -n 1 <~/stream-bak.json | jget collection | jvals | tac)
		#entries=$(tls api-v2.soundcloud.com <<-EOF 2>/dev/null | tail -n 1 |\
				jget collection | jvals | tac
		#	GET /stream${offset:+?offset=}$offset HTTP/1.1
		#	host: api-v2.soundcloud.com
		#	Connection: close
		#	Authorization: OAuth $token
		#	
		#	EOF
		# )$entries
		firstuuid=$(printf %s "$entries" | head -n 1 | jget uuid)
		if test ! $prevuuid || test $(strcmp $firstuuid $prevuuid) -le 0; then
			break;
		else
			offset=$firstuuid
		fi
	done
	# Show each entry
	while read -r entry; do
		uuid=$(printf %s "$entry" | jget uuid)
		if test ! $prevuuid || test $(strcmp $uuid $prevuuid) -gt 0; then
			# Display entry
			type=$(printf %s "$entry" | jget type)
			artistinfo=$(printf %s "$entry" | jget ${type%-repost} user)
			if grep $(printf %s "$artistinfo" | jget id) <~/sctoskip >/dev/null; then
				artist=$(printf %s "$artistinfo" | jget username | jdecode)
				printf 'skipping %s...\n' "$artist"
				continue
			fi
			if test $type = track-repost || test $type = playlist-repost; then
				reposter=$(printf %s "$entry" | jget user username | jdecode)
				printf 'Repost by %s:\n' "$reposter"
			fi
			info=$(printf %s "$entry" | jget ${type%-repost})
			username=$(printf %s "$info" | jget user username | jdecode)
			title=$(printf %s "$info" | jget title | jdecode)
			printf '%s - [4m%s[0m' "$username" "$title"
			if test $type = track || test $type = track-repost; then
				seconds=$(($(printf %s "$info" | jget duration) / 1000))
				printf ' (%s:%02s)\n' $((seconds / 60)) $((seconds % 60))
			else
				tracks=$(printf %s "$info" | jget tracks)
				count=$(printf %s "$tracks" | jvals | wc -l | tr -d '[:space:]')
				printf ' (%s tracks)\n' "$count"
			fi
			# Do commands for that entry
			while true; do
				test $savec || c=$(getch)
				unset savec
				case "$c" in
					d )
						youtube-dl "$(printf %s "$info" | jget permalink_url)" ;;
					o )
						printf %s "$info" | jget permalink_url | tr -d '\n' | pbcopy ;;
					t )
						# Have user edit description of mix into google search lines
						mixdesc=$(printf %s "$info" | jget description | jdecode)
						printf '%s\n' "$mixdesc" >/tmp/scstream.$$.playlist
						${EDITOR:-vi} /tmp/scstream.$$.playlist </dev/tty
						# Search google for each track to play
						while read -r search; do
							echo "$search"
							echo "$search" | pbcopy
							test "$(getch)" =  && break
							#playurl "$(googlepick "$search")" 3
						done </tmp/scstream.$$.playlist ;;
					' ' )
						playurl "$(printf %s "$info" | jget permalink_url | jdecode)" ;;
					 )
						exit ;;
					 )
						break ;;
					* )
						cmd "$c" ;;
				esac
			done
			prevuuid=$uuid
			test "$1" && echo $prevuuid >"$1"
			wroteone=1
		fi
	done <<-EOF
		$entries
		EOF
	test $wroteone || sleep 7200
done
