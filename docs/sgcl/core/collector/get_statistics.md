[sgcl](../../README.md) › [core](../README.md) › [collector](../collector.md)

# sgcl::collector::get_statistics

```cpp
static statistics get_statistics() noexcept;
```

The counters of the collector's work ([statistics](../collector-statistics.md)), read without stopping it and
without waiting for anything: every field is a relaxed load of a counter the collector thread stores at the end of
a cycle. The values describe the last cycle that completed, except `committed_bytes` and `live_bytes`, which are
read now. Before the first cycle every counter is zero.

## Parameters

None.

## Return value

The counters, a [statistics](../collector-statistics.md) by value.

## Complexity

Constant: a few atomic reads.

## Exceptions

None.

## Notes

It never waits for the collector, so it may be called anywhere: from a destructor, between the gates of a
[stepper](../collector-stepper.md), in a loop that watches the collector.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<tracked_ptr<int>> kept;
    for (int i : range(100000)) {
        kept.push_back(make_tracked<int>(i));
    }
    // optional, for the demonstration only: the collector runs its cycles by itself
    collector::force_collect(true);

    auto s = collector::get_statistics();
    println("{} cycles ({} full), {} objects, {} MB live, {} MB committed", s.cycles,
            s.full_cycles, s.live_objects, s.live_bytes / 1048576, s.committed_bytes / 1048576);
    println("last cycle {} ms with {} helpers", s.last_cycle_ms, s.last_helpers_used);
    for (int i : range(8)) {
        println("  {} {} ms", collector::phase_names[i], s.phases_ms[i]);
    }
}
```

Sample output:

```text
5 cycles (2 full), 100001 objects, 2 MB live, 4 MB committed
last cycle 1.108875 ms with 0 helpers
  registration 0.001708 ms
  states 0 ms
  roots 0.150375 ms
  marking 0.947917 ms
  updated 0.008 ms
  sweep 0.000458 ms
  release 0.000167 ms
  trim 8.3e-05 ms
```

## See also

- [statistics](../collector-statistics.md): the counters
- [get_type_statistics](get_type_statistics.md): the live objects by type
- [sgcl::collector](../collector.md)
