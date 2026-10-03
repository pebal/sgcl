[sgcl](../README.md) › [async](README.md)

# sgcl::async::on_unhandled

```cpp
#include "sgcl/async/coroutine.h"   // or "sgcl/async.h"

namespace sgcl::async {
    void (*on_unhandled(void (*handler)(std::exception_ptr)) noexcept)(std::exception_ptr);
}
```

Sets what becomes of an exception that a task let go of threw and nobody reads — a task started by [go](go.md),
[detached](task/detach.md), or whose object was dropped or assigned over after it started — or that the call of
[go_blocking](go_blocking.md) threw, and returns the handler it replaces; `nullptr` puts the default back.

The handler is called once per such exception, on the thread where it is found: the worker that ends the task, the
thread that detaches or drops a task already done, the collector's for a task held by an object it collects, or the
thread of the blocking pool that ran the call of `go_blocking`. It must not throw. The default is Go's panic in a
goroutine: one line on stderr with the exception's type, its `what()` and where the task ended (a worker's index, the
blocking pool, or the thread's number), then `std::terminate`:

```text
sgcl::async: unhandled exception in a detached task on worker 3: std::system_error: connection reset
```

Where the task was started is not kept by its frame, so the line cannot say it. An exception that
[result](task/result.md), [wait](task/wait.md) or `co_await` gave to someone is theirs, never the handler's; one of a
task that lost a race ([when_any](when_any.md), [with_timeout](with_timeout.md), [with_deadline](with_deadline.md))
is dropped with the loser, as its value is.

## Parameters

| Parameter | Description |
|---|---|
| `handler` | the function called with each exception nobody reads; `nullptr` for the default |

## Return value

The handler in place before the call: the default one when none was set.

## Complexity

Constant: one exchange.

## Exceptions

None.

## Notes

A server that is to log what a handler let go of threw and go on sets a handler of its own at its start; the handler
is one for the process, read by every thread.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include <stdexcept>

using namespace sgcl;

void log_it(std::exception_ptr e) {
    try {
        std::rethrow_exception(e);
    } catch (const std::exception& x) {
        println("a task let go of threw: {}", x.what());
    }
}

async::task<> fails() {
    throw std::runtime_error("connection reset");
    co_return;
}

int main() {
    async::on_unhandled(log_it);
    {
        async::task<> t = fails();
        t.resume();  // ends with the exception, kept in the frame
    }  // let go of unread: the handler is called here, on this thread

    async::task<> read = fails();
    read.resume();
    try {
        read.result();  // given to someone: never the handler's
    } catch (const std::exception& x) {
        println("read: {}", x.what());
    }
    println("{}", async::on_unhandled(nullptr) == &log_it);
}
```

Output:

```text
a task let go of threw: connection reset
read: connection reset
true
```

## See also

- [go](go.md), [detach](task/detach.md): the tasks let go of
- [go_blocking](go_blocking.md): a blocking call let go of
- [task](task/README.md): what a task does with what it throws
- [task_group](task_group/README.md): children whose first exception the wait for the group rethrows
