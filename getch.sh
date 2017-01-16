#!/bin/sh
stty raw -echo
dd bs=1 count=1 2>/dev/null
stty -raw echo
