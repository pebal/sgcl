[sgcl](../README.md) › [async](README.md) › [blocking_pool](blocking_pool/README.md)

# sgcl::async::blocking_pool::statistics

```cpp
#include "sgcl/async/blocking.h"   // or "sgcl/async.h"

namespace sgcl::async {
    struct blocking_pool {
        struct statistics {
            unsigned threads = 0;
            unsigned idle = 0;
            size_t queued = 0;
        };
    };
}
```

`sgcl::async::blocking_pool::statistics` is the counts of the blocking pool, what
[get_statistics](blocking_pool/get_statistics.md) returns: a plain struct, its fields read together under the pool's
lock. Before the first job, and after a [stop](blocking_pool/stop.md), every count is zero.

## Member objects

| Field | Description |
|---|---|
| `threads` | the threads of the pool now; 0 when none, or the pool not started |
| `idle` | of them, the ones parked with nothing to do |
| `queued` | the jobs waiting for a thread |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::event started, release;
    auto job = async::spawn_blocking([started, release] {
        started.set();
        release.wait();  // holds its thread
    });
    started.wait();
    async::blocking_pool::set_threads(1);
    auto queued = async::spawn_blocking([] {});  // no thread left: it waits
    async::blocking_pool::statistics s = async::blocking_pool::get_statistics();
    println("{} threads, {} idle, {} queued", s.threads, s.idle, s.queued);
    release.set();
    queued.wait();
}
```

Output:

```text
1 threads, 0 idle, 1 queued
```

## See also

- [get_statistics](blocking_pool/get_statistics.md): what returns it
- [scheduler::statistics](scheduler-statistics.md): the same for the scheduler
- [sgcl::async::blocking_pool](blocking_pool/README.md)
