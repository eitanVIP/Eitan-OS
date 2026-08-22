find src programs -type f \( -name "*.c" -o -name "*.h" -o -name "*.S" \) \
  | grep -E '^src/|^programs/' \
  | grep -v '^src/programs/' \
  | grep -v '^src/compiled' \
  | xargs wc -l | sort -n