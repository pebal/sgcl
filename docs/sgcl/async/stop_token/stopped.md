[sgcl](../../README.md) › [async](../README.md) › [stop_token](README.md)

# sgcl::async::stop_token::stopped

```cpp
auto stopped() const noexcept;
```

Waits for the stop, and gives nothing: `co_await token.stopped()` in a task suspends it until the stop is
requested, holding no thread; `token.stopped().wait()` blocks a thread the same way. A stop requested before the
wait ends it at once. The wait is the receive of the token's channel, which the stop closes.

The token must have a source (an assertion in debug builds): [stop_possible](stop_possible.md) says.

## Parameters

None.

## Return value

An [operation](../operation/README.md), marked nodiscard, carried out by `co_await token.stopped()` in a task or
`token.stopped().wait()` on a thread; either gives nothing and returns once the stop has been requested.

## Complexity

Constant. Carried out: constant, a waiter on the channel's list while the stop has not come.

## Exceptions

None from the call. Carried out, none: the wait is a receive on the token's channel, which the library never
sends on, so it wakes no one ([channel](channel.md#notes)).

## Notes

A task that waits for the stop alone lives until the stop comes: the channel's list of waiters holds its frame.
To wait for the stop or something else, whichever comes first, [on_stop](on_stop.md) is a case of a
[select](../select.md).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<string> guard(async::stop_token token) {
    co_await token.stopped();
    co_return "the task saw the stop";
}

int main() {
    async::stop_source source;
    auto t = async::spawn(guard(source.token()));
    source.request_stop();
    source.token().stopped().wait();  // stopped already: at once
    println("{}", t.wait());
}
```

Output:

```text
the task saw the stop
```

## See also

- [on_stop](on_stop.md): the stop as a case of a select
- [stop_requested](stop_requested.md): a look without a wait
- [channel](channel.md): the channel the wait receives on
- [sgcl::async::stop_token](README.md)
