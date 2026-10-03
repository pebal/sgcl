[sgcl](../../README.md) › [async](../README.md) › [event](README.md)

# sgcl::async::event::on_set

```cpp
template<class F>
auto on_set(F f) const noexcept(std::is_nothrow_move_constructible_v<F>);
```

Makes a case of a [select](../select.md) that is served once the event is set: when the select takes the case,
`f()` is called. The case is a receive on the event's channel, served by its close, so a select may wait for an
event beside a channel, a timer or a stop, and an event set already serves it at once, every time.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the body of the case, called with no arguments once the event is set |

## Return value

A case of a [select](../select.md), of a type of the library. The select waits until one of its cases can be
served; when it takes this one, `f()` runs. With [otherwise](../otherwise.md) beside it, an event not set is not
waited for.

## Complexity

Constant to make; served, a look at the channel.

## Exceptions

What the move constructor of `F` throws; none when it is noexcept. When the case is served, what `f` throws comes
out of the select.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::event cancelled;
    auto look = [&] {
        async::select(
            cancelled.on_set([] { println("cancelled"); }),
            async::otherwise([] { println("go on"); })
        ).wait();
    };
    look();
    cancelled.set();
    look();
    look();
}
```

Output:

```text
go on
cancelled
cancelled
```

## See also

- [select](../select.md): the cases and how one is chosen
- [wait, operator co_await](wait.md): the wait outside a select
- [set](set.md): what serves the case
- [sgcl::async::event](README.md)
