#!/bin/sh
# Kill all background jobs and exit with non-zero status.
sigint()
{
	jobs -p | xargs kill
	exit 1
}

# Set the variable named $2 to the $2 tag from flac file $1.
readtag()
{
	tagentry=$(metaflac --show-tag=$2 "$1")
	eval "$2=\$(printf "%s" \"\${tagentry#*=}\")"
}

# Recursively copy everything from $1 to $2, concurrently transcoding flac files to mp3 with lame
# args $3 and ignoring m3u files.
transcodedir()
{
	for file in "$1"/*; do
		if test -d "$file"; then
			mkdir "$2/$file"
			echo "$2/$file"
			transcodedir "$file" "$2" "$3"
		else
			case "$file" in
				*.flac )
					readtag "$file" artist
					readtag "$file" album
					readtag "$file" title
					readtag "$file" tracknumber
					readtag "$file" date
					out=$2/${file%flac}mp3
					flac -sdc "$file" | lame -S $3 -q 2 --id3v2-only \
						--ta "${artist:?}" \
						--tl "${album:?}" \
						--tt "${title:?}" \
						--tn ${tracknumber:?} \
						${date:+--ty $date} \
						- "$out" 2>/dev/null &&
						echo "$out" ||
						echo "transcode: failed to encode '$file'" >&2 & ;;
				*.m3u ) ;;  # ignore m3u files
				* )
					cp "$file" "$2/$file"
					echo "$2/$file"
			esac
		fi
	done
	wait
}


if test $# -lt 3; then
	echo "usage: transcode indir outdirtemplate quality [quality ...]" >&2
	exit 1
else
	template=$2
	if test $(echo "$template" | head -c 1) != /; then
		outroot=$(pwd)/
	fi
	cd "$1"
	shift 2
	for arg; do
		case "$arg" in
			320 )
				lamearg="-b 320" ;;
			V0 )
				lamearg="-V 0" ;;
			V2 )
				lamearg="-V 2" ;;
			* )
				echo "transcode: unknown quality '$arg'" >&2
				exit 1
		esac
		outdir=$outroot$(printf "$template" $arg)
		mkdir "$outdir"
		echo "$outdir"
		trap sigint INT
		transcodedir . "$outdir" "$lamearg"
	done
fi
