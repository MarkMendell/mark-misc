#!/bin/sh
entries=$(tls api-v2.soundcloud.com <<-EOF | tail -n 1
	GET /stream HTTP/1.1
	host: api-v2.soundcloud.com
	Connection: close
	Authorization: OAuth $(cat)
	
	EOF)
