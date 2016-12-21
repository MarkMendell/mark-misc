#/bin/sh
sum=0
count=0
while read x; do
	sum=$((sum + x))
	count=$((count + 1))
done
echo "scale=${1:-2}; $sum / $count" | bc
