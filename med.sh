#!/bin/sh
sorted=$(sort -n)
count=$(echo "$sorted" | wc -l)
echo "$sorted" | sed "$((count / 2 + 1))q;d"
