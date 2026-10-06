# SGCL: a C++20 application platform
# Copyright (c) 2022-2026 Sebastian Nibisz
# SPDX-License-Identifier: Apache-2.0
#
# The reference of bench_config's ini_parse (benchmarks/encoding/config.cpp):
# Python's configparser in the dialect ini.h states (strict, no
# interpolation, keys in their case, a blank line ending a value), reading the
# same document, which bench_config writes when asked:
#   bench_config sgcl ini_parse 0 dump > bench.ini
#   python3 benchmarks/python/ini.py bench.ini [seconds=2]
# Prints nanoseconds per document and megabytes of it per second, in
# bench_config's form. The document's keys before the first section are
# configparser's error: a section of a name no text has is put before them,
# as tools/ini_oracle.py does.
import configparser
import sys
import time

text = open(sys.argv[1], 'rb').read().decode('utf-8')
seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 2.0
doc = '[\x01global]\n' + text


def parse():
    p = configparser.RawConfigParser(strict=True, empty_lines_in_values=False, interpolation=None,
                                     default_section='\x00none', comment_prefixes=('#', ';'),
                                     inline_comment_prefixes=None)
    p.optionxform = str
    p.read_string(doc)
    return p


for _ in range(2):
    parse()
count = 0
t0 = time.perf_counter()
while time.perf_counter() - t0 < seconds:
    parse()
    count += 1
ns = (time.perf_counter() - t0) / count * 1e9
size = len(text.encode('utf-8'))
print('configparser op=ini_parse bytes=%d count=%d ns/op=%.0f MB/s=%.0f' % (size, count, ns, size / ns * 1e3))
