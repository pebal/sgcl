[sgcl](../README.md) › [async](README.md) › [scheduler](scheduler.md)

# sgcl::async::scheduler::statistics

```cpp
#include "sgcl/async/scheduler.h"   // or "sgcl/async.h"

namespace sgcl::async {
    struct scheduler {
        struct statistics {
            unsigned workers = 0;
            size_t global_queued = 0;
            size_t local_queued = 0;
            unsigned spinning = 0;
            unsigned sleeping = 0;
        };
    };
}
```

`sgcl::async::scheduler::statistics` is the queues and the workers of the scheduler as they are, what
[get_statistics](scheduler/get_statistics.md) returns, for a benchmark or a monitor: a plain struct. The counts are
read at different moments, so they are a snapshot of each, not one consistent state. Before the first start, and
after a [stop](scheduler/stop.md), every count is zero.

## Member objects

| Field | Description |
|---|---|
| `workers` | the threads of the pool; 0 when the scheduler is not running |
| `global_queued` | the tasks on the global queue |
| `local_queued` | the tasks on the workers' rings and next slots, together |
| `spinning` | the workers looking for work |
| `sleeping` | the workers asleep in the kernel |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::scheduler::set_workers(4);
    async::scheduler::workers();  // started
    async::scheduler::statistics s = async::scheduler::get_statistics();
    println("{} workers: {} looking, {} asleep", s.workers, s.spinning, s.sleeping);
    println("{} tasks queued", s.global_queued + s.local_queued);
}
```

Sample output:

```text
4 workers: 0 looking, 0 asleep
0 tasks queued
```

## See also

- [get_statistics](scheduler/get_statistics.md): what returns it
- [blocking_pool::statistics](blocking_pool-statistics.md): the same for the blocking pool
- [sgcl::async::scheduler](scheduler.md)
