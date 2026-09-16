#!/bin/sh
# Builds example.cpp with debug info, runs LLDB on it with the formatters
# loaded, prints the variables at the marked line and checks the output.
#   lldb/check.sh [c++ compiler=clang++]
set -e
CXX=${1:-clang++}
DIR=$(cd "$(dirname "$0")" && pwd)
OUT=$(mktemp -d)
"$CXX" -std=c++20 -O0 -g -I"$DIR/.." -o "$OUT/example" "$DIR/example.cpp"
LINE=$(grep -n "// BREAK" "$DIR/example.cpp" | cut -d: -f1)
lldb -b -o "command script import $DIR/sgcl.py" -o "b example.cpp:$LINE" -o "process launch -X false" \
     -o "frame variable node kept plain owned weak v vp a d l f m s um us at" "$OUT/example" > "$OUT/out.txt" 2>&1 || true
fail=0
check() {
    if ! grep -qF -- "$1" "$OUT/out.txt"; then
        echo "missing: $1"
        fail=1
    fi
}
check '(sgcl::tracked_ptr<Node>) node = 0x'
check 'cell 0 of block 0x'
check 'v = 8'
check '(sgcl::unique_ptr<Node>) owned = 0x'
check 'UniqueLock'
check '(sgcl::weak_ptr<Node>) weak = 0x'
check '(sgcl::vector<int>) v = size=3 capacity='
check '[2] = 3'
check '[1] = null'
check '(sgcl::array<int>) a = size=3'
check '[0] = 5'
check '(sgcl::deque<int>) d = size=3'
check '[0] = 9'
check '[1] = "bb"'
check '(sgcl::forward_list<int>) f = size=2'
check '(first = 1, second = "one")'
check '(sgcl::set<int>) s = size=3'
check '(first = 1, second = 10)'
check '[0] = "x"'
if [ $fail = 0 ]; then
    echo "lldb formatters: ok"
else
    echo "--- output:"; cat "$OUT/out.txt"
fi
rm -rf "$OUT"
exit $fail
