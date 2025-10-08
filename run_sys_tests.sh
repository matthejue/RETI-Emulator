#!/usr/bin/env bash

./extract_input_and_expected.sh $2

num_tests=0;
not_running_through=();
not_passed=();

if [[ $2 == "all" ]]; then
  paths=(./sys_test/*.reti)
elif [[ -n "$2" ]]; then
  paths=(./sys_test/*$2*.reti)
else
  paths=(./sys_test/{basic,special,example,error}*.reti)
fi

if [ ! -f "${paths[0]}" ]; then
  exit 1
fi

for test in "${paths[@]}"; do
  ./heading_subheadings.py "heading" "$test" "$1" "="
  ./bin/reti_emulator_main $(cat ./opts/test_opts.txt) $3 "$test";

  if [[ $? != 0 ]]; then
    not_running_through+=("$test");
  fi;

  diff "${test%.reti}.expected_output" "${test%.reti}.output"
  if [[ $? != 0 ]]; then
    not_passed+=("$test");
  fi
  ((num_tests++));
done;
echo Running through: $(($num_tests-${#not_running_through[@]})) / $num_tests | tee -a ./sys_test/test_results
echo Not running through: ${not_running_through[*]} | tee -a ./sys_test/test_results
echo Passed: $(($num_tests-${#not_passed[@]})) / $num_tests | tee -a ./sys_test/test_results
echo Not passed: ${not_passed[*]} | tee -a ./sys_test/test_results

if [[ ${#not_passed[@]} != 0 ]]; then
    exit 1
fi
