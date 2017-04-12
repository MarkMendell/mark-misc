#!/bin/sh
export PS1=''
shedpath=${SHEDPATH:-~/shed}
export ENV=$shedpath/.profile
export PATH=$shedpath:$PATH
export SHED=/tmp/shed.$$
mkdir $SHED
touch $SHED/{buf,file}
test -f "$1" && { cat <"$1" >$SHED/buf; printf '%s\n' "$1" >$SHED/file; }
sh -o noglob -o allexport
