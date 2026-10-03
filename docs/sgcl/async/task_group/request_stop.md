[sgcl](../../README.md) › [async](../README.md) › [task_group](README.md)

# sgcl::async::task_group::request_stop

```cpp
void request_stop();
```

Requests the stop of the whole scope by hand: the group's source is stopped, its token's channel closed, every wait
on the token woken, and the sources made from it stopped. The children see it through [token](token.md) and leave;
none is stopped from outside. A stop is not an error: a group stopped this way waits without throwing, unless a
child threw.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the waits on the token and the sources made from it, each woken or stopped once.

## Exceptions

`std::system_error` when the wake of a waiting task must start the scheduler's workers and a thread cannot be
started ([README: The rules](../README.md#the-rules), 5).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> poller(async::stop_token tok, atomic<int>& left) {
    co_await tok.stopped();
    ++left;
}

int main() {
    atomic<int> left = 0;
    async::task_group g;
    for (int i : range(3)) {
        g.go(poller(g.token(), left));
    }
    g.request_stop();
    g.wait();  // no exception: a stop is not an error
    println("{} left, stop requested: {}", left.load(), g.stop_requested());
}
```

Output:

```text
3 left, stop requested: true
```

## See also

- [token](token.md): what the children look at
- [stop_requested](stop_requested.md): whether the stop has been requested
- [sgcl::async::task_group](README.md)
