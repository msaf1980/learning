#!/bin/sh

[ -z "$1" -o -z "$2" ] && {
	echo "use: $0 in out" >&2
	exit 1
}
[ "$1" == "$2" ] && {
	echo "in file equal to out" >&2
	exit 1
}

sed -e '/^#/d' "$1" | sort -k1 > "$2"
