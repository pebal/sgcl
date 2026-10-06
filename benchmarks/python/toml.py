# SGCL: a C++20 application platform
# Copyright (c) 2022-2026 Sebastian Nibisz
# SPDX-License-Identifier: Apache-2.0
#
# The reference of bench_toml (benchmarks/encoding/toml.cpp): Python's tomllib
# reading the same document, which bench_toml writes when asked:
#   bench_toml sgcl parse twitter 0 dump > twitter.toml
#   python3 benchmarks/python/toml.py twitter.toml [seconds=2]
# Prints nanoseconds per document and megabytes of it per second, in
# bench_toml's form.
import sys
import time
import tomllib

path = sys.argv[1]
seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 2.0
text = open(path, 'rb').read().decode('utf-8')
for _ in range(2):
    tomllib.loads(text)
count = 0
t0 = time.perf_counter()
while time.perf_counter() - t0 < seconds:
    tomllib.loads(text)
    count += 1
ns = (time.perf_counter() - t0) / count * 1e9
size = len(text.encode('utf-8'))
print('tomllib op=parse corpus=%s bytes=%d count=%d ns/op=%.0f MB/s=%.0f' % (path, size, count, ns, size / ns * 1e3))
