#!/bin/sh
# A fuzzing campaign with libFuzzer (Homebrew's LLVM: brew install llvm) on
# one harness written to its ABI (tests/<module>/fuzz/<name>_fuzz.cpp),
# under ASan and UBSan:
#
#   tests/fuzz/run.sh tests/net/http/fuzz/http_head_fuzz.cpp 600 [libFuzzer options...]
#
# The corpus grows in build-fuzz/corpus/<name>/ (outside the repository),
# started from the harness's seeds (a seeds/<name>/ directory beside it);
# a crash is written to build-fuzz/crash/<name>/, to be kept as a
# regression file beside the seeds. The dictionary is tests/fuzz/dict/<name>.dict
# when there is one. The collector's own heap is not LSan's to judge
# (-detect_leaks=0), and -fork re-executes the binary rather than
# forking a process whose collector has threads.
set -e
src=$1
seconds=${2:-60}
shift 2 || shift $#
root=$(cd "$(dirname "$0")/../.." && pwd)
name=$(basename "$src" _fuzz.cpp)
llvm=${SGCL_LLVM:-/opt/homebrew/opt/llvm}
out=$root/build-fuzz
mkdir -p "$out/bin" "$out/corpus/$name" "$out/crash/$name"
# compiled and linked in two steps: in one, clang runs dsymutil over the
# -g binary, which grew past 40 GB on the LZMA harness; SGCL_FUZZ_LIBS
# (include paths and libraries) goes to both, the half each does not use
# ignored
flags="-std=c++20 -O1 -g -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=undefined -fsanitize-address-use-after-return=never -Qunused-arguments"
"$llvm/bin/clang++" $flags -I"$root" ${SGCL_FUZZ_LIBS} -c "$root/$src" -o "$out/bin/$name.o"
"$llvm/bin/clang++" $flags "$out/bin/$name.o" ${SGCL_FUZZ_LIBS} -o "$out/bin/$name"
seeds=$(dirname "$root/$src")/seeds/$name
dict=$root/tests/fuzz/dict/$name.dict
set -- -max_total_time="$seconds" -timeout=5 -rss_limit_mb=4096 -detect_leaks=0 -use_value_profile=1 \
    -artifact_prefix="$out/crash/$name/" -print_final_stats=1 "$@"
[ -f "$dict" ] && set -- -dict="$dict" "$@"
exec "$out/bin/$name" "$@" "$out/corpus/$name" $( [ -d "$seeds" ] && echo "$seeds" )
