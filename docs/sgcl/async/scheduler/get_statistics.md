[sgcl](../../README.md) › [async](../README.md) › [scheduler](README.md)

# sgcl::async::scheduler::get_statistics

```cpp
static statistics get_statistics();
```

Returns the queues and the workers as they are, for a benchmark or a monitor. It starts nothing: before the first
start, and after a [stop](stop.md), every count is 0. The counts are a snapshot of different words read at
different moments, not one consistent state.

## Parameters

None.

## Return value

A [scheduler::statistics](../scheduler-statistics.md): the workers, the tasks on the global queue and on the workers'
own queues, the workers looking for work and the ones asleep.

## Complexity

Linear in the number of workers.

## Exceptions

`std::system_error` when the scheduler's lock cannot be taken, as `std::mutex::lock` reports it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> answer() {
    co_return 42;
}

int main() {
    auto before = async::scheduler::get_statistics();
    println("before the first task: {} workers", before.workers);

    async::scheduler::set_workers(2);
    async::spawn(answer()).wait();
    auto now = async::scheduler::get_statistics();
    println("now: {} workers, {} tasks queued", now.workers, now.global_queued + now.local_queued);
    println("{} looking, {} asleep", now.spinning, now.sleeping);
}
```

Sample output:

```text
before the first task: 0 workers
now: 2 workers, 0 tasks queued
1 looking, 0 asleep
```

## See also

- [workers](workers.md): the number of workers, the scheduler started
- [blocking_pool::get_statistics](../blocking_pool/get_statistics.md): the same for the blocking pool
- [sgcl::async::scheduler](README.md)
