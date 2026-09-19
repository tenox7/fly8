#!/bin/sh
# Fly8.app launcher. The game writes its macros file and log next to its
# data, so the data is kept in a per-user directory; user edited files
# (ini, macros, button maps) are copied only once.

DIR="$(cd "$(dirname "$0")" && pwd)"
RES="$DIR/../Resources"
DATA="$HOME/Library/Application Support/Fly8"

mkdir -p "$DATA"
for f in "$RES"/*; do
	b="$(basename "$f")"
	case "$b" in
	*.icns) ;;
	*.ini|*.max|*.mac|*.adv|*.b50|*.f22)
		[ -e "$DATA/$b" ] || cp "$f" "$DATA/" ;;
	*)	cp -f "$f" "$DATA/" ;;
	esac
done

cd "$DATA" && exec "$DIR/fly8-bin" "F$DATA" "$@"
