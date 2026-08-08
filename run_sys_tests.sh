#!/usr/bin/env bash

NOT_PASSED_TESTS_FILE="./config/not_passed_tests.txt"
MAX_EMULATOR_DURATION_SECONDS=5

use_not_passed_tests=false

interrupted() {
  echo
  echo "Interrupted by Ctrl+C. Stopping test run." >&2
  exit 130
}

trap interrupted INT TERM

usage() {
  cat <<EOF
Usage:
  $0 [OPTIONS] COLUMNS [TEST_PATTERN] [EXTRA_EMU_ARGS]

Options:
  --not-passed
      Run only the whitespace-separated test paths listed in:
      ${NOT_PASSED_TESTS_FILE}

      TEST_PATTERN is ignored when this option is active.

  -h, --help
      Show this help message.

Examples:
  $0 120 all
  $0 120 basic
  $0 --not-passed 120

Each emulator invocation is stopped automatically after
${MAX_EMULATOR_DURATION_SECONDS} seconds.

After the test run, tests whose output did not match the expected output,
failed to run, or timed out are written as whitespace-separated paths to:
  ${NOT_PASSED_TESTS_FILE}
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --not-passed)
      use_not_passed_tests=true
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    --)
      shift
      break
      ;;
    -*)
      echo "Unknown option: $1" >&2
      usage >&2
      exit 2
      ;;
    *)
      break
      ;;
  esac
done

if [[ $# -lt 1 ]]; then
  echo "Error: COLUMNS argument is required." >&2
  usage >&2
  exit 2
fi

columns="$1"
test_pattern="${2:-}"
extra_emu_args="${3:-}"

paths=()

if [[ "$use_not_passed_tests" == true ]]; then
  if [[ ! -f "$NOT_PASSED_TESTS_FILE" ]]; then
    echo "Test list file not found: $NOT_PASSED_TESTS_FILE" >&2
    exit 1
  fi

  mapfile -t paths < <(
    tr -s '[:space:]' '\n' < "$NOT_PASSED_TESTS_FILE" |
      sed '/^[[:space:]]*$/d'
  )
elif [[ "$test_pattern" == "all" ]]; then
  paths=(./system_test/*.reti)
elif [[ -n "$test_pattern" ]]; then
  paths=(./system_test/*"$test_pattern"*.reti)
else
  paths=(./system_test/{basic,special,example,error}*.reti)
fi

if [[ ${#paths[@]} -eq 0 ]]; then
  echo "No test paths were found." >&2
  exit 1
fi

for test in "${paths[@]}"; do
  if [[ ! -f "$test" ]]; then
    echo "Test file not found: $test" >&2
    exit 1
  fi
done

if [[ "$use_not_passed_tests" == true ]]; then
  # The helper accepts a test pattern instead of an array of paths.
  # Therefore, process each selected test using its exact basename.
  for test in "${paths[@]}"; do
    exact_test_pattern="$(basename "${test%.reti}")"
    ./extract_input_and_expected.sh "$exact_test_pattern"

    helper_status=$?

    if [[ $helper_status -eq 130 ]]; then
      interrupted
    elif [[ $helper_status -ne 0 ]]; then
      echo "Helper exited with status $helper_status for $test" >&2
      exit "$helper_status"
    fi
  done
else
  ./extract_input_and_expected.sh "$test_pattern"

  helper_status=$?

  if [[ $helper_status -eq 130 ]]; then
    interrupted
  elif [[ $helper_status -ne 0 ]]; then
    echo "Helper exited with status $helper_status" >&2
    exit "$helper_status"
  fi
fi

num_tests=0
not_running_through=()
not_passed=()

for test in "${paths[@]}"; do
  ./heading_subheadings.py "heading" "$test" "$columns" "="

  heading_status=$?

  if [[ $heading_status -eq 130 ]]; then
    interrupted
  elif [[ $heading_status -ne 0 ]]; then
    echo "Heading helper exited with status $heading_status for $test" >&2
    exit "$heading_status"
  fi

  # Remove an output file from a previous run so stale output cannot
  # accidentally make a failed or timed-out test appear successful.
  rm -f "${test%.reti}.output"

  # The unquoted expansions intentionally permit multiple options.
  # shellcheck disable=SC2046,SC2086
  timeout \
    "${MAX_EMULATOR_DURATION_SECONDS}s" \
    ./binary/reti_emulator_main \
    $(cat ./config/test_opts.txt) \
    $extra_emu_args \
    "$test"

  emulator_status=$?

  if [[ $emulator_status -eq 130 ]]; then
    interrupted
  fi

  if [[ $emulator_status -eq 124 ]]; then
    echo \
      "Emulator timed out after ${MAX_EMULATOR_DURATION_SECONDS}s for $test"
    not_running_through+=("$test")
  elif [[ $emulator_status -ne 0 ]]; then
    echo "Emulator exited with status $emulator_status for $test"
    not_running_through+=("$test")
  fi

  output_status=0

  if [[ $emulator_status -eq 0 ]]; then
    if [[ -f "${test%.reti}.output" ]]; then
      diff \
        "${test%.reti}.expected_output" \
        "${test%.reti}.output"

      output_status=$?

      if [[ $output_status -eq 130 ]]; then
        interrupted
      fi
    else
      echo "Emulator did not create an output file for $test"
      output_status=1
    fi
  else
    output_status=1
  fi

  if [[ $output_status -ne 0 ]]; then
    not_passed+=("$test")
  fi

  ((num_tests++))
done

# Overwrite the file with the tests that did not pass during this run.
# The paths are written on one line, separated by spaces.
if [[ ${#not_passed[@]} -eq 0 ]]; then
  : > "$NOT_PASSED_TESTS_FILE"
else
  (
    IFS=' '
    printf '%s\n' "${not_passed[*]}"
  ) > "$NOT_PASSED_TESTS_FILE"
fi

echo \
  "Running through: $((num_tests - ${#not_running_through[@]})) / $num_tests" |
  tee -a ./system_test/test_results

echo "Not running through: ${not_running_through[*]}" |
  tee -a ./system_test/test_results

echo "Passed: $((num_tests - ${#not_passed[@]})) / $num_tests" |
  tee -a ./system_test/test_results

echo "Not passed: ${not_passed[*]}" |
  tee -a ./system_test/test_results

echo "Updated test list: $NOT_PASSED_TESTS_FILE"

if [[ ${#not_passed[@]} -ne 0 ]]; then
  exit 1
fi
