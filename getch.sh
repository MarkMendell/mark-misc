#!/bin/sh
stty raw -echo
dd bs=3 count=1 2>/dev/null
stty -raw echo
