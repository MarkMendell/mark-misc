#!/bin/sh
cp sd9p.c sd9p.macos.c
patch <sd9p.macos.diff sd9p.macos.c
gcc sd9p.macos.c
