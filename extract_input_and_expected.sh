#!/usr/bin/env bash

if [[ $1 == "all" ]]; then
  paths=(./sys_test/*.reti)
elif [[ -n "$1" ]]; then
  paths=(./sys_test/*$1*.reti)
else
  paths=(./sys_test/{basic,special,error}*.reti)
fi

for test in "${paths[@]}"; do
  if [[ $(sed -n "1p" "$test") =~ ^#\ output: ]]; then
    sed -n '1p' "$test" | tr '\t' ' ' | sed -e 's/^# output: *//' | tr '\n' ' ' > "${test%.reti}.expected_output"
  elif [[ $(sed -n "2p" "$test") =~ ^#\ output: ]]; then
    sed -n '2p' "$test" | tr '\t' ' ' | sed -e 's/^# output: *//' | tr '\n' ' ' > "${test%.reti}.expected_output"
  elif [[ $(sed -n "3p" "$test") =~ ^#\ output: ]]; then
    sed -n '3p' "$test" | tr '\t' ' ' | sed -e 's/^# output: *//' | tr '\n' ' ' > "${test%.reti}.expected_output"
  fi
done
