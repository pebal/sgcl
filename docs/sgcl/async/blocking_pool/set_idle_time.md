[sgcl](../../README.md) › [async](../README.md) › [blocking_pool](README.md)

# sgcl::async::blocking_pool::set_idle_time

```cpp
static void set_idle_time(duration d);
```

Sets how long a thread of the pool that finds the queue empty waits for a job before it exits, over
`config::blocking_idle_milliseconds`. A thread reads it when it parks, so the threads parked at the call keep the time
they parked with. A short time gives the threads back soon after a burst of blocking calls; a long one keeps them for
the next burst.

## Parameters

| Parameter | Description |
|---|---|
| `d` | the idle time |

## Return value

None.

## Complexity

Constant.

## Exceptions

`std::system_error` when the pool's lock cannot be taken, as `std::mutex::lock` reports it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    async::blocking_pool::set_idle_time(20ms);
    async::spawn_blocking([] { return 0; }).wait();
    async::blocking_pool::wait_idle();
    println("after the job: {} threads", async::blocking_pool::get_statistics().threads);
    this_thread::sleep_for(200ms);
    println("after the idle time: {} threads", async::blocking_pool::get_statistics().threads);
}
```

Output:

```text
after the job: 1 threads
after the idle time: 0 threads
```

## See also

- [idle_time](idle_time.md): the time now
- [set_threads](set_threads.md): the cap on the threads
- [sgcl::async::blocking_pool](README.md)
