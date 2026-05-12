#!/bin/sh
# Mirror SRC into DST, swapping Y and Z in *.map files so TrenchBroom's
# Z-up coordinates land in the engine's Y-up convention. Brush plane
# points "( x y z )" and Valve-format UV axes "[ x y z offset ]" are
# rewritten in place, as are vec3-valued entity keys (see VEC3_KEYS
# below — extend when adding new vec3 properties to the FGD); everything
# else is copied verbatim.
set -eu

src=${1:?usage: $0 SRC DST}
dst=${2:?usage: $0 SRC DST}

VEC3_KEYS='origin|normal|velocity|angles|mangle'

mkdir -p "$dst"
find "$src" -mindepth 1 | while IFS= read -r path; do
    rel=${path#"$src"/}
    out=$dst/$rel
    if [ -d "$path" ]; then
        mkdir -p "$out"
    elif [ "${path##*.}" = "map" ]; then
        sed -E '
            s/\( *([^ )]+) +([^ )]+) +([^ )]+) *\)/( \1 \3 \2 )/g
            s/\[ *([^] ]+) +([^] ]+) +([^] ]+) +([^] ]+) *\]/[ \1 \3 \2 \4 ]/g
            s/"('"$VEC3_KEYS"')" "([^ "]+) +([^ "]+) +([^ "]+)"/"\1" "\2 \4 \3"/g
        ' "$path" > "$out"
    else
        cp "$path" "$out"
    fi
done
