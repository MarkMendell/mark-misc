#!/bin/sh
{
	stty -icanon -echo -icrnl -isig
	dd bs=8 count=1 2>/dev/null
	stty icanon echo icrnl isig
} </dev/tty
