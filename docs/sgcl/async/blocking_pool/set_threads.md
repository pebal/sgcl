[sgcl](../../README.md) › [async](../README.md) › [blocking_pool](README.md)

# sgcl::async::blocking_pool::set_threads

```cpp
static void set_threads(unsigned n);
```

Sets the most threads the pool grows to from now on, over `SGCL_BLOCKING_THREADS` and `config::blocking_threads`;
0 puts the default back, the larger of 64 and four times the hardware concurrency. A smaller cap stops the growth at
once: a job that finds every thread busy waits in the queue. The threads over it are not stopped: they exit as they
run out of work, after the idle time.

## Parameters

| Parameter | Description |
|---|---|
| `n` | the cap; 0 for the default |

## Return value

None.

## Complexity

Constant.

## Exceptions

`std::system_error` when the pool's lock cannot be taken, as `std::mutex::lock` reports it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    async::blocking_pool::set_threads(2);
    vector<async::blocking_task<int>> jobs;
    for (int i : range(6)) {
        jobs.push_back(async::spawn_blocking([i] {
            this_thread::sleep_for(10ms);
            return i;
        }));
    }
    int sum = 0;
    for (auto& job : jobs) {
        sum += job.wait();
    }
    println("sum {}, on {} threads", sum, async::blocking_pool::get_statistics().threads);
}
```

Output:

```text
sum 15, on 2 threads
```

## See also

- [max_threads](max_threads.md): the cap now
- [set_idle_time](set_idle_time.md): how long an idle thread stays
- [sgcl::async::blocking_pool](README.md)
