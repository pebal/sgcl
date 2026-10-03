[sgcl](../../README.md) › [async](../README.md) › [semaphore](../semaphore.md)

# sgcl::async::semaphore::on_acquire

```cpp
template<class F>
auto on_acquire(F f) noexcept(std::is_nothrow_move_constructible_v<F>);
```

Makes a case of a [select](../select.md) that takes a permit: when the select takes the case, a permit is taken and
`f()` is called. The case is the channel's receive of a signal, served when a permit is free, so a select may wait
for a permit beside a channel, a timer or a stop.

The permit stays taken when `f` returns: the program gives it back with [release](release.md).

## Parameters

| Parameter | Description |
|---|---|
| `f` | the body of the case, called with no arguments once the permit is taken |

## Return value

A case of a [select](../select.md), of a type of the library. The select waits until one of its cases can be
served; when it takes this one, a permit is taken and `f()` runs. With [otherwise](../otherwise.md) beside it, a
semaphore without a free permit is not waited for.

## Complexity

Constant to make. Served, a receive through the channel's ring.

## Exceptions

What the move constructor of `F` throws; none when it is noexcept. When the case is served, what `f` throws comes
out of the select, and the permit is taken.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    async::semaphore slots(2);
    for (int i : range(3)) {
        async::select(
            slots.on_acquire([] { println("a permit"); }),
            async::otherwise([] { println("none free"); })
        ).wait();
    }
    println("{}", slots.available());
}
```

Output:

```text
a permit
a permit
none free
0
```

## See also

- [select](../select.md): the cases and how one is chosen
- [acquire](acquire.md): the permit outside a select
- [release](release.md): gives the permit back
- [sgcl::async::semaphore](../semaphore.md)
