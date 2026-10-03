[sgcl](../../README.md) › [async](../README.md) › [stop_source](README.md)

# sgcl::async::stop_source::request_stop

```cpp
void request_stop();
```

Requests the stop. The token's channel is closed: every wait on it, in a thread or a task, ends at once, and the
selects waiting with an `on_stop` case are served by it and run its body. Then the deadline armed, if any, is
cancelled, and the children are stopped, in no particular order, each the same way. A second request, or one
after a deadline or a parent's stop, does nothing.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the waiters woken and in the children, and their descendants, stopped.

## Exceptions

`std::system_error` when a wake starts the scheduler's workers and a worker cannot be started.

## Notes

The stop of a source does not stop a task: a task sees the stop through its token and stops itself.
[stop_requested](stop_requested.md) reads `true` before any waiter woken by the stop runs.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<int> idle(async::stop_token token) {
    co_await token.stopped();
    co_return 1;
}

int main() {
    async::stop_source server;
    vector<async::task<int>> tasks;
    for (int i : range(3)) {
        async::stop_source connection(server.token());
        tasks.push_back(async::spawn(idle(connection.token())));
    }
    server.request_stop();  // the children too, and the tasks see it
    int stopped = 0;
    for (auto& t : tasks) {
        stopped += t.wait();
    }
    println("{} stopped", stopped);
}
```

Output:

```text
3 stopped
```

## See also

- [stop_after](stop_after.md), [stop_at](stop_at.md): the stop by a timer
- [stop_token::on_stop](../stop_token/on_stop.md), [stop_token::stopped](../stop_token/stopped.md): the waits it
  ends
- [sgcl::async::stop_source](README.md)
