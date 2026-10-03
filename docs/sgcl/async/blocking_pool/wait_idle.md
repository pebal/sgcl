[sgcl](../../README.md) › [async](../README.md) › [blocking_pool](README.md)

# sgcl::async::blocking_pool::wait_idle

```cpp
static void wait_idle();
```

Blocks the calling thread until every job queued so far has run, the jobs whose handles were dropped included. The
threads stay: it waits for the work, not for the pool to stop.

## Parameters

None.

## Return value

None.

## Complexity

Constant, besides the wait for the jobs.

## Exceptions

`std::system_error` when the pool's lock cannot be taken, as `std::mutex::lock` reports it.

## Notes

It blocks the calling thread, so it is not called from a task on a worker: debug builds assert.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    atomic<int> written = 0;
    for (int i : range(5)) {
        async::spawn_blocking([&written] {  // the handle dropped: the job runs all the same
            this_thread::sleep_for(5ms);
            ++written;
        });
    }
    async::blocking_pool::wait_idle();
    println("{} jobs ran", written.load());
}
```

Output:

```text
5 jobs ran
```

## See also

- [stop](stop.md): the jobs run and the threads joined
- [sgcl::async::blocking_pool](README.md)
