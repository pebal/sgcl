[sgcl](../README.md) › [core](README.md) › [collector](collector/README.md)

# sgcl::collector::statistics

```cpp
#include "sgcl/core/collector.h"   // or "sgcl/core.h"

namespace sgcl {
    class collector {
    public:
        struct statistics {
            size_t cycles;
            size_t full_cycles;
            size_t live_objects;
            size_t live_bytes;
            size_t committed_bytes;
            double last_cycle_ms;
            unsigned helper_threads;
            unsigned last_helpers_used;
            bool helpers_enabled;
            double phases_ms[8];
        };
    };
}
```

`sgcl::collector::statistics` is the counters of the collector's work, what
[get_statistics](collector/get_statistics.md) returns: a plain struct, read without stopping the collector and
without waiting for anything. The values describe the last cycle that completed, except `committed_bytes` and
`live_bytes`, which are read now. Before the first cycle every counter is zero.

## Member objects

| Field | Description |
|---|---|
| `cycles` | the cycles completed since the start |
| `full_cycles` | of which full: all of them unless the collection is generational ([config](config.md): `generational`) |
| `live_objects` | the objects marked by the last cycle |
| `live_bytes` | the managed memory in use by the allocators now: the pages in use, garbage not yet swept included, so at least what the live objects take |
| `committed_bytes` | the managed memory committed now, as [get_committed_memory](collector/get_committed_memory.md) |
| `last_cycle_ms` | the wall time of the last cycle, in milliseconds |
| `helper_threads` | the helper threads started so far |
| `last_helpers_used` | of which the last cycle used |
| `helpers_enabled` | whether the helpers are on for the next cycle ([config](config.md): `helpers_growth_threshold`) |
| `phases_ms` | the wall time of each phase of the last cycle, in milliseconds, named by `collector::phase_names[i]`: registration, states, roots, marking, updated states, sweep, page release, trim. The eight add up to `last_cycle_ms`, give or take the clock. |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<tracked_ptr<int>> kept;
    for (int i : range(10000)) {
        kept.push_back(make_tracked<int>(i));
    }
    // optional, for the demonstration only: the collector runs its cycles by itself
    collector::force_collect(true);
    collector::statistics s = collector::get_statistics();

    double phases = 0;
    for (double ms : s.phases_ms) {
        phases += ms;
    }
    println("{} cycles, {} full, last {:.3f} ms, the phases {:.3f} ms", s.cycles, s.full_cycles,
            s.last_cycle_ms, phases);
    println("{} objects, {} KB live, {} KB committed", s.live_objects, s.live_bytes / 1024,
            s.committed_bytes / 1024);
}
```

Sample output:

```text
3 cycles, 2 full, last 0.258 ms, the phases 0.258 ms
10001 objects, 960 KB live, 4096 KB committed
```

## See also

- [get_statistics](collector/get_statistics.md): what returns it
- [type_statistics](collector-type_statistics.md): the live objects by type
- [sgcl::collector](collector/README.md)
