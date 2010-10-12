#!/bin/sh

usage ()
{
	name=${0##*/}
	echo
	echo "$name <mapped file|mapping name> <SMAPS field> <SMAPS snapshot file>"
	echo
	echo "Shows size-sorted totals for given mapping for all the processes"
	echo "in the given SMAPS snapshot file."
	echo
	echo "SMAPS field should be one of:"
	echo "  Size, Rss, Pss, Shared_Clean, Shared_Dirty,"
	echo "  Private_Clean, Private_Dirty, Referenced, Swap."
	echo
	echo "Examples:"
	echo "- what processes use most RAM:"
	echo "  $name '.*' Pss smaps.cap"
	echo "- what processes are most on swap:"
	echo "  $name '.*' Swap smaps.cap"
	echo "- what processes have largest heaps:"
	echo "  $name '\[heap\]' Size smaps.cap"
	echo
	echo "ERROR: $1!"
	echo
	exit 1
}

if [ $# -ne 3 ]; then
	usage "wrong number of arguments"
fi
if [ -z $(echo "$2"|fgrep -e Size -e Rss -e Pss -e Shared_Clean -e Shared_Dirty -e Private_Clean -e Private_Dirty -e Referenced -e Swap) ]; then
	usage "unknown SMAPS mapping value type"
fi
if [ \! -f "$3" ]; then
	usage "file '$2' doesn't exist"
fi

mapping="$1"
field="$2"
file="$3"

heading="Size:\t\tPID:\tName:\n"

echo "finding process totals for type '$field' in '$mapping' mappings..."
echo
printf $heading

awk '
function mapping_usage () {
	if (size) {
		printf("%5d kB\t%5d\t%s\n", size, pid, name);
		size = 0;
	}
}
/^#Name/ {
	mapping_usage();
	name = $2;
}
/^#Pid/ {
	mapping_usage();
	pid = $2;
}
# hex address range, stuff, mapping name
/^[0-9a-f]+-[0-9a-f].*'"$mapping"'/ {
	mapping = 1;
	map = $6;
}
/^'"$field"':/ {
	if (mapping > 0) {
		mapping = 0;
		size += $2;
	}
}
END {
	mapping_usage();
}
' "$file"|sort -n
printf $heading

if [ \! -z "$(echo \"$mapping\"|fgrep -e '[' -e ']' -e '*' -e '+' -e '.')" ]; then
	echo
	echo "WARNING: mapping name '$mapping' contained regular expression special characters."
	echo "If results look unexpected, make sure these characters were escaped properly!"
fi