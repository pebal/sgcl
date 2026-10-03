[sgcl](../../README.md) › [async](../README.md) › [task_group](README.md)

# sgcl::async::task_group::on_done

```cpp
template<class F>
auto on_done(F f) noexcept(std::is_nothrow_move_constructible_v<F>);
```

Makes a case of a [select](../select.md) that is served once every child has finished: when the select takes the
case, `f()` is called. The case is the [on_done](../wait_group/on_done.md) of the group's count, made on the round
current when `on_done` is called; a group that never started a child serves it at once. The exception of a child,
if any, is not given to the case: the [wait](wait.md) that follows rethrows it, and returns at once.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the body of the case, called with no arguments once every child has finished |

## Return value

A case of a [select](../select.md), of a type of the library. The select waits until one of its cases can be
served; when it takes this one, `f()` runs. With [otherwise](../otherwise.md) beside it, a group with children
running is not waited for.

## Complexity

Constant to make; served, a look at the channel of the group's count.

## Exceptions

What the move constructor of `F` throws; none when it is noexcept. When the case is served, what `f` throws comes
out of the select.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> job(async::event go) {
    co_await go;
}

int main() {
    async::event go;
    async::task_group g;
    g.go(job(go));
    auto look = [&] {
        async::select(
            g.on_done([] { println("all finished"); }),
            async::otherwise([&] { println("{} running", g.count()); })
        ).wait();
    };
    look();
    go.set();
    async::select(g.on_done([] { println("all finished"); })).wait();  // waits for the child
    g.wait();  // no exception to rethrow: returns at once
}
```

Output:

```text
1 running
all finished
```

## See also

- [select](../select.md): the cases and how one is chosen
- [wait, operator co_await](wait.md): the wait outside a select, which rethrows
- [sgcl::async::task_group](README.md)
