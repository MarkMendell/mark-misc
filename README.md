# avg
`avg [precision]`  
reads newline-separated integers and outputs their average to `precision` digits past the decimal (default of 2).
```
$ printf '5\n10\n-2\n' | avg 1
4.3
```

# btls
`btls`  
reads torrent formatted input and lists the files included.
```
$ btls < car.torrent
wheels/front.whl
wheels/back.whl
engine.eng
```

# cpx
`cpx from to`  
copies `from` to `to` and makes `to` executable. 
```
$ cpx avg.sh /bin/avg
```

# lread
`lread`  
waits for a line from the terminal between printing lines from stdin.
```
$ printf '5\n10\n-2\n' | lread
5   # <ENTER>
10  # <ENTER>
-2  # <ENTER>
```

# med
`med`  
reads newline-separated integers and outputs their median, choosing the lower value if there is an even count.
```
$ printf '5\n10\n-2\n' | med
5
```

# mixtape.moe
`mixtape.moe file`  
uploads `file` to mixtape.moe and outputs the url. Requires bash (for now), grep with -o (for now), and curl.
```
$ mixtape.moe evidence.zip
https://my.mixtape.moe/tymyko.zip
```

# transcode
`transcode indir outdirtemplate quality [quality ...]`  
copies files recursively from `indir` to `$(printf "$outdirtemplate" $quality)` for each `quality` (320, V0, or V2), replacing flac files with `quality` mp3 transcodes and ignoring m3u files. Requires metaflac, flac, and lame.
```
$ transcode "TGS × Maltine REMIX (FLAC)" /tmp/"TGS x Maltine REMIX (%s)" V0 V2
/tmp/TGS x Maltine REMIX (V0)
/tmp/TGS x Maltine REMIX (V0)/./cover.jpg
/tmp/TGS x Maltine REMIX (V0)/./scans
/tmp/TGS x Maltine REMIX (V0)/./scans/front.png
/tmp/TGS x Maltine REMIX (V0)/./scans/back.png
/tmp/TGS x Maltine REMIX (V0)/./05 おんなじキモチ (Hercelot Remix).mp3
/tmp/TGS x Maltine REMIX (V0)/./03 ゆうやけハナビ (banvox Remix).mp3
/tmp/TGS x Maltine REMIX (V0)/./06 Limited addiction (okadada Remix).mp3
/tmp/TGS x Maltine REMIX (V0)/./01 Liar (RE-NDZ Remix).mp3
/tmp/TGS x Maltine REMIX (V0)/./02 ヒマワリと星屑 (Yoshino Yoshikawa ''Pollarstars'' Remix).mp3
/tmp/TGS x Maltine REMIX (V0)/./04 Rock You! (tofubeats 1988 dub version).mp3
/tmp/TGS x Maltine REMIX (V0)/./07 キラリ☆ (Avec Avec Remix).mp3
/tmp/TGS x Maltine REMIX (V2)
/tmp/TGS x Maltine REMIX (V2)/./cover.jpg
/tmp/TGS x Maltine REMIX (V0)/./scans
/tmp/TGS x Maltine REMIX (V0)/./scans/front.png
/tmp/TGS x Maltine REMIX (V0)/./scans/back.png
/tmp/TGS x Maltine REMIX (V2)/./05 おんなじキモチ (Hercelot Remix).mp3
/tmp/TGS x Maltine REMIX (V2)/./06 Limited addiction (okadada Remix).mp3
/tmp/TGS x Maltine REMIX (V2)/./03 ゆうやけハナビ (banvox Remix).mp3
/tmp/TGS x Maltine REMIX (V2)/./01 Liar (RE-NDZ Remix).mp3
/tmp/TGS x Maltine REMIX (V2)/./02 ヒマワリと星屑 (Yoshino Yoshikawa ''Pollarstars'' Remix).mp3
/tmp/TGS x Maltine REMIX (V2)/./04 Rock You! (tofubeats 1988 dub version).mp3
/tmp/TGS x Maltine REMIX (V2)/./07 キラリ☆ (Avec Avec Remix).mp3
```
