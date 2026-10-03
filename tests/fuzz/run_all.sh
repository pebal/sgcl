#!/bin/sh
# Every libFuzzer harness of the tree (tests/**/fuzz/*_fuzz.cpp), one after
# the other, each for the same number of seconds through run.sh, and a
# summary: the runs, the coverage, the corpus and whether anything was
# found. The whole review of the fuzzing is one command:
#
#   tests/fuzz/run_all.sh 300              every harness, 300 s each
#   tests/fuzz/run_all.sh 60 compress      the harnesses whose path holds "compress"
#
# The libraries a harness's oracle needs are read from its includes:
# <openssl/...> (Homebrew's openssl@3), <zlib.h>, <bzlib.h>, <lzma.h>
# (Homebrew's xz), <unicode/...> (Homebrew's icu4c), <pcre2.h> (Homebrew's
# pcre2, with SGCL_FUZZ_PCRE2 defined, which turns the oracle on);
# SGCL_FUZZ_LIBS, when set, is added to them. Each harness's output goes to
# build-fuzz/logs/<name>.log; a crash to build-fuzz/crash/<name>/, as
# run.sh has it. Every run is bounded by libFuzzer's -max_total_time (and
# -timeout=5 per input), so the script ends after about seconds × harnesses
# plus the builds. The exit status is 1 when any harness found something or
# failed to build.
set -u
seconds=${1:-60}
filter=${2:-}
root=$(cd "$(dirname "$0")/../.." && pwd)
out=$root/build-fuzz
mkdir -p "$out/logs"
brew=${SGCL_BREW:-/opt/homebrew/opt}
harnesses=$(cd "$root" && find tests -path '*/fuzz/*_fuzz.cpp' | sort)
failed=0
summary=$out/logs/summary.txt
printf '%-28s %-8s %12s %8s %8s %s\n' harness result runs cov corpus note > "$summary"
for src in $harnesses; do
    case "$src" in
        *"$filter"*) ;;
        *) continue ;;
    esac
    name=$(basename "$src" _fuzz.cpp)
    libs=${SGCL_FUZZ_LIBS:-}
    if grep -q '#include <openssl/' "$root/$src"; then
        libs="$libs -I$brew/openssl@3/include -L$brew/openssl@3/lib -lcrypto"
    fi
    if grep -q '#include <zlib.h>' "$root/$src"; then
        libs="$libs -lz"
    fi
    if grep -q '#include <bzlib.h>' "$root/$src"; then
        libs="$libs -lbz2"
    fi
    if grep -q '#include <lzma.h>' "$root/$src"; then
        libs="$libs -I$brew/xz/include -L$brew/xz/lib -llzma"
    fi
    if grep -q '#include <unicode/' "$root/$src"; then
        libs="$libs -I$brew/icu4c/include -L$brew/icu4c/lib -licuuc -licui18n"
    fi
    if grep -q '#include <pcre2.h>' "$root/$src"; then
        libs="$libs -DSGCL_FUZZ_PCRE2 -I$brew/pcre2/include -L$brew/pcre2/lib -lpcre2-8"
    fi
    log=$out/logs/$name.log
    crashes_before=$(ls "$out/crash/$name" 2>/dev/null | wc -l | tr -d ' ')
    printf 'fuzz %s (%s s)\n' "$src" "$seconds"
    SGCL_FUZZ_LIBS="$libs" sh "$root/tests/fuzz/run.sh" "$src" "$seconds" > "$log" 2>&1
    status=$?
    crashes_after=$(ls "$out/crash/$name" 2>/dev/null | wc -l | tr -d ' ')
    runs=$(sed -n 's/^stat::number_of_executed_units: *//p' "$log" | tail -1)
    last=$(grep -E '^#[0-9]+' "$log" | tail -1)
    cov=$(printf '%s\n' "$last" | sed -n 's/.* cov: \([0-9]*\).*/\1/p')
    corpus=$(printf '%s\n' "$last" | sed -n 's/.* corp: \([0-9]*\).*/\1/p')
    note=
    result=ok
    if ! grep -q '^INFO: Seed:' "$log"; then
        # libFuzzer never started: the build (or the link) failed
        result=BUILD
        note=$(grep -m1 'error' "$log" | cut -c1-80)
        failed=1
    elif [ "$status" -ne 0 ] || [ "$crashes_after" -gt "$crashes_before" ]; then
        result=FOUND
        note="exit $status, $((crashes_after - crashes_before)) new in build-fuzz/crash/$name/"
        failed=1
    fi
    printf '%-28s %-8s %12s %8s %8s %s\n' "$name" "$result" "${runs:--}" "${cov:--}" "${corpus:--}" "$note" >> "$summary"
done
echo
cat "$summary"
exit $failed
