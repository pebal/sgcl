[sgcl](../../README.md) › [async](../README.md) › [scheduler](../scheduler.md)

# sgcl::async::scheduler::workers

```cpp
static unsigned workers();
```

Returns the number of worker threads, starting the scheduler when it is not running. The number is the one
[set_workers](set_workers.md) set, else `SGCL_WORKERS` from the environment (read once, at the first start), else
`config::workers`; 0 in any of them is the hardware concurrency, and there are at most 64.

## Parameters

None.

## Return value

The number of workers of the running scheduler.

## Complexity

Constant once the scheduler runs; the first call starts the workers.

## Exceptions

`std::system_error` when the scheduler has to start and a worker thread cannot be started; the scheduler is left
stopped, with no worker running, and the next call tries again.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::scheduler::set_workers(3);
    println("{} workers", async::scheduler::workers());
}
```

Output:

```text
3 workers
```

## See also

- [set_workers](set_workers.md): sets the number
- [get_statistics](get_statistics.md): the workers and the queues, without starting anything
- [sgcl::async::scheduler](../scheduler.md)
