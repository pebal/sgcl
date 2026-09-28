# Sums the "objects created" and "mem allocs" of the collector's log lines
# between a case's BEGIN and END (bench_heap_io_net, built with
# SGCL_LOG_PRINT_LEVEL=2) and prints each END line with them, per call.
import re
import sys

created = pages = 0
for line in open(sys.argv[1]):
    if line.startswith("BEGIN"):
        created = pages = 0
        continue
    m = re.search(r"mem allocs:\s*(\d+),.*objects created:\s*(\d+)", line)
    if m:
        pages += int(m.group(1))
        created += int(m.group(2))
        continue
    if line.startswith("END"):
        reps = int(re.search(r"reps=(\d+)", line).group(1))
        print("%s  objects %.2f/op  new pages %d" % (line.rstrip().rsplit(" reps=", 1)[0], created / reps, pages))
    elif "failed" in line or "refused" in line:
        print(line.rstrip())
