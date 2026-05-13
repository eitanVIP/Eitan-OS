echo "[**] Compiling fonts"

if [[ -z "$1" ]]; then
  echo "Usage: ./compile.sh <font_name>"
  exit 1
fi

xxd -i fonts/$1.psf > src/compiled_fonts/$1.c
printf '#include "%s.h"\n\n%s\n\nunsigned char* %s_font_get() {\n    return fonts_%s_psf;\n}' $1 "$(cat src/compiled_fonts/$1.c)" $1 $1 > src/compiled_fonts/$1.c
echo "[**] Created c file with binary"
printf "#pragma once\n\nunsigned char* %s_font_get();" $1 > src/compiled_fonts/$1.h
echo "[**] Created h file"