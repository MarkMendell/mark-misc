# [avg](../../raw/master/avg.sh)
`avg [precision]`  
reads newline-separated integers and outputs their average to `precision` digits past the decimal (default of 2).
```
$ printf '5\n10\n-2\n' | avg 1
4.3
```

# [btls](../../raw/master/btls.c)
`btls`  
reads torrent formatted input and lists the files included.
```
$ btls < car.torrent
wheels/front.whl
wheels/back.whl
engine.eng
```

# [cpx](../../raw/master/cpx.sh)
`cpx from to`  
copies `from` to `to` and makes `to` executable. 
```
$ cpx avg.sh /bin/avg
```

# [lread](../../raw/master/lread.sh)
`lread`  
waits for a line from the terminal between printing lines from stdin.
```
$ printf '5\n10\n-2\n' | lread
5   # <ENTER>
10  # <ENTER>
-2  # <ENTER>
```

# [med](../../raw/master/med.sh)
`med`  
reads newline-separated integers and outputs their median, choosing the lower value if there is an even count.
```
$ printf '5\n10\n-2\n' | med
5
```

# [mixtape.moe](../../raw/master/mixtape.moe.bash)
`mixtape.moe file`  
uploads `file` to mixtape.moe and outputs the url. Requires [bash](https://www.gnu.org/software/bash/) (for now), grep with -o (for now), and [curl](https://curl.haxx.se/).
```
$ mixtape.moe evidence.zip
https://my.mixtape.moe/tymyko.zip
```

# [ready](../../raw/master/ready.c)
`ready`  
exits when input is ready to be read.
```
$ { sleep 5; echo hello; } | { echo hi; cat; }
hi  # instant
hello  # after 5 seconds
$ { sleep 5; echo hello; } | { ready; echo hi; cat; }
hi  # after 5 seconds
hello  # instant
```

# [tls](../../raw/master/tls.sh)
`tls [-r] host [port]`  
sends input to `host:port` (default 443) over TLS and outputs responses, reconnecting automatically if `-r` is specified. Requires [openssl](https://www.openssl.org/) and [ready](#ready).
```
$ printf "GET /user/mmendell/ HTTP/1.1\r\nhost: www.andrew.cmu.edu\r\n\r\n" | tls www.andrew.cmu.edu
HTTP/1.1 200 OK
Content-Length: 84
Content-Type: text/html

<head><meta charset="utf-8" /></head>
<body>
<a href="ha">Fusion HA</a>
♫
</body>
```

# [transcode](../../raw/master/transcode.sh)
`transcode indir outdirtemplate quality [quality ...]`  
copies files recursively from `indir` to `$(printf "$outdirtemplate" $quality)` for each `quality` (320, V0, or V2), replacing flac files with `quality` mp3 transcodes and ignoring m3u files. Requires [flac + metaflac](https://xiph.org/flac/index.html) and [lame](http://lame.sourceforge.net/).
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

# [urldecode](../../raw/master/urldecode.c)
`urldecode`  
outputs its input urldecoded.
```
$ echo %5bErno%cc%83_Lendvai%5d_Bela_Bartok_An_Analysis_of_His_Mu.pdf | urldecode
[Ernõ_Lendvai]_Bela_Bartok_An_Analysis_of_His_Mu.pdf
```

# [zippyshare](../../raw/master/zippyshare.sh)
`zippyshare [-s] url`  
outputs the file hosted at the zippyshare index page `url`. If `-s` is specified, output is saved to a file named from the download url instead. Requires [curl](https://curl.haxx.se/) and [urldecode](#urldecode).
```
$ zippyshare http://www120.zippyshare.com/v/fi89FJi1/file.html > by-request-only.zip
```
