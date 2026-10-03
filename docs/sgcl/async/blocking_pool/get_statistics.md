[sgcl](../../README.md) › [async](../README.md) › [blocking_pool](README.md)

# sgcl::async::blocking_pool::get_statistics

```cpp
static statistics get_statistics();
```

Returns the counts of the pool as they are, read under the pool's lock. It starts nothing: before the first job, and
after a [stop](stop.md), every count is 0.

## Parameters

None.

## Return value

A [blocking_pool::statistics](../blocking_pool-statistics.md): the threads, the idle ones among them, the jobs waiting
for a thread.

## Complexity

Constant, besides the count of the queue, linear in the jobs waiting.

## Exceptions

`std::system_error` when the pool's lock cannot be taken, as `std::mutex::lock` reports it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

void show(const char* when) {
    auto st = async::blocking_pool::get_statistics();
    println("{}: {} threads, {} idle, {} queued", when, st.threads, st.idle, st.queued);
}

int main() {
    show("before");
    async::spawn_blocking([] { return 1; }).wait();
    async::blocking_pool::wait_idle();
    show("after a job");
}
```

Output:

```text
before: 0 threads, 0 idle, 0 queued
after a job: 1 threads, 1 idle, 0 queued
```

## See also

- [max_threads](max_threads.md): the cap on the threads
- [scheduler::get_statistics](../scheduler/get_statistics.md): the same for the workers
- [sgcl::async::blocking_pool](README.md)
