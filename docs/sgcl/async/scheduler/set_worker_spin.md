[sgcl](../../README.md) › [async](../README.md) › [scheduler](../scheduler.md)

# sgcl::async::scheduler::set_worker_spin

```cpp
static void set_worker_spin(duration d) noexcept;
```

Sets how long a worker with nothing to run looks for work before it sleeps in the kernel, over
`SGCL_WORKER_SPIN_US` and `config::worker_spin_microseconds` (20 µs). A task made ready within that window costs no
wake through the kernel; a longer spin buys that for a busy program with cores it does not need for anything else.
The setting applies at once, with the workers running or not, and to the thread of an [executor](../executor.md),
which spins as long before it parks.

## Parameters

| Parameter | Description |
|---|---|
| `d` | the time to look; kept in whole microseconds, a negative one as zero |

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    println("{}", async::scheduler::worker_spin());
    async::scheduler::set_worker_spin(100us);
    println("{}", async::scheduler::worker_spin());
    async::scheduler::set_worker_spin(0s);  // straight to sleep
    println("{}", async::scheduler::worker_spin());
}
```

Output:

```text
20µs
100µs
0s
```

## See also

- [worker_spin](worker_spin.md): the time now
- [set_workers](set_workers.md): the number of workers
- [sgcl::async::scheduler](../scheduler.md)
