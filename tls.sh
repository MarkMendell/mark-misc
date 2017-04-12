#!/bin/sh
openssl s_client -quiet -connect $1:${2:-443}
