#!/usr/bin/env bash

if [[ $1 == "all" ]]; then
  paths=(./sys_test/*.reti)
elif [[ -n "$1" ]]; then
  paths=(./sys_test/*$1*.reti)
else
  paths=(./sys_test/{basic,special,example,error}*.reti)
fi

for test in "${paths[@]}"; do
  output_line=$(
    sed -n '1,3p' "$test" \
      | awk '/^# output:/ {
          sub(/^# output:[ \t]?/, "")
          gsub(/\t/, " ")
          printf "%s", $0
          exit
        }'
  )

  if [[ -n "$output_line" ]]; then
    printf '%s' "$output_line" > "${test%.reti}.expected_output"
  fi
done
