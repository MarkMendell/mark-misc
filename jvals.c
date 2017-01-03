#include <stdio.h>


int
main(void)
{
	int hascolons = getcharordie() == '{';
	int depth = 0;
	int c = getcharordie();
	do {
