#!/bin/sh
if [ $# -eq 0 ]; then
  echo "usage: backup file [...]" >&2
  exit 1
elif [ ! "$BACKUP_FILE" ]
  BACKUP_FILE=~/backups.txt
  echo "backup: BACKUP_FILE not set, using '$BACKUP_FILE'" >&2
fi

tmp_dir=$(tmpdir)
for file; do
  tar_file="$tmpdir/$(basename -- "$file").tar.gz"
  tar cz "$file" > "$tar_file"
  if url=$(mixtape.moe "$file"); then
    echo "$(date +%m/%d/%Y) $url $(abspath "file")" >> "$BACKUP_FILE"
    echo "backup: backed up '$file'"
  else
    echo "backup: failed to backup '$file'" >&2
    rm -r "$tmp_dir"
    exit 1
  fi
done
rm -r "$tmp_dir"
