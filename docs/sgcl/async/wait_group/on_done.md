[sgcl](../../README.md) › [async](../README.md) › [wait_group](../wait_group.md)

# sgcl::async::wait_group::on_done

```cpp
template<class F>
auto on_done(F f) const noexcept(std::is_nothrow_move_constructible_v<F>);
```

Makes a case of a [select](../select.md) that is served once the count is zero: when the select takes the case,
`f()` is called. The case is a receive on the channel of the round current when `on_done` is called, served by its
close, so a select may wait for a group beside a channel, a timer or a stop. A group that never counted up serves
it at once: the channel of its first round is closed from the start.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the body of the case, called with no arguments once the count is zero |

## Return value

A case of a [select](../select.md), of a type of the library. The select waits until one of its cases can be
served; when it takes this one, `f()` runs. With [otherwise](../otherwise.md) beside it, a group whose count is not
zero is not waited for.

## Complexity

Constant to make; served, a look at the round's channel.

## Exceptions

What the move constructor of `F` throws; none when it is noexcept. When the case is served, what `f` throws comes
out of the select.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::wait_group g;
    auto look = [&] {
        async::select(
            g.on_done([] { println("all done"); }),
            async::otherwise([&] { println("{} to go", g.count()); })
        ).wait();
    };
    look();
    g.add(2);
    look();
    g.done();
    g.done();
    look();
}
```

Output:

```text
all done
2 to go
all done
```

## See also

- [select](../select.md): the cases and how one is chosen
- [wait, operator co_await](wait.md): the wait outside a select
- [sgcl::async::wait_group](../wait_group.md)
