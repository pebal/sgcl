[sgcl](../../README.md) › [async](../README.md) › [stop_token](../stop_token.md)

# sgcl::async::stop_token::channel

```cpp
receive_channel<void> channel() const noexcept;
```

Returns the receiving end of the channel the stop closes: a handle to the channel of signals inside the stop's
state, which it keeps alive. What the module does with a channel it receives on it does with this one: a receive on
it ends, `false`, when the stop is requested; `on_receive(f)` is a case of a [select](../select.md); a function that
takes a [receive_channel](../receive_channel.md)`<void>` to end its work on takes this one. [on_stop](on_stop.md)
and [stopped](stopped.md) are this channel's case and receive under names of their own.

The token must have a source (an assertion in debug builds): [stop_possible](stop_possible.md) says.

## Parameters

None.

## Return value

The receiving end of the stop's channel, a [receive_channel](../receive_channel.md)`<void>`: closed when the stop
is requested, never sent on by the library.

## Complexity

Constant.

## Exceptions

None.

## Notes

The channel is the stop's own, and the handle has no `close()` and no `send()`: a close of it would read as a stop
that stopped neither the children of the source nor its deadline, and a send would wake a task in
[stopped](stopped.md) with no stop requested. The stop is requested through the [stop_source](../stop_source.md).

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::stop_source source;
    async::receive_channel<void> stop = source.token().channel();
    println("{}", stop.closed());
    source.request_stop();
    println("{}", stop.closed());
    println("{}", stop.receive().wait());  // closed: at once, with nothing
}
```

Output:

```text
false
true
false
```

## See also

- [on_stop](on_stop.md): the case of a select on this channel
- [stopped](stopped.md): the wait for the stop
- [receive_channel](../receive_channel.md): what is returned
- [sgcl::async::stop_token](../stop_token.md)
