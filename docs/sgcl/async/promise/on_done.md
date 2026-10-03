[sgcl](../../README.md) › [async](../README.md) › [promise](../promise.md)

# sgcl::async::promise\<T\>::on_done

```cpp
template<class F>
auto on_done(F f) const noexcept(std::is_nothrow_move_constructible_v<F>);
```

The set as a case of a [select](../select.md): the case is served once the promise is set, with a value or with an
exception, and its body `f()` is called then, after the wait. The body reads the value with [result](result.md),
which returns at once by then and rethrows what [set_exception](set_exception.md) set. A promise set already serves
the case at once. With a [timeout](../timeout.md) case beside it, the wait for a completion is bounded; with a stop
token's [on_stop](../stop_token/on_stop.md), it is cancelled. The same for `promise<void>`.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the body of the case, `void()` |

## Return value

The case, a receive case over the promise's channel, to be passed to [select](../select.md) by value. Nothing is
waited for until the select is carried out, and a case is for one select.

## Complexity

Constant: `f` is moved into the case.

## Exceptions

What the move constructor of `F` throws; none when it is noexcept. When the case is served, what the body throws
comes out of the select.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;
using namespace std::chrono_literals;

async::task<int> wait_briefly(async::promise<int> p) {
    int v = -1;
    co_await async::select(
        p.on_done([&] { v = p.result(); }),
        async::timeout(10ms, [] { println("gave up"); }));
    co_return v;
}

int main() {
    async::promise<int> set, never;
    set.set_value(7);
    println("{}", async::spawn(wait_briefly(set)).wait());
    println("{}", async::spawn(wait_briefly(never)).wait());
}
```

Output:

```text
7
gave up
-1
```

## See also

- [wait, operator co_await](wait.md): the wait alone
- [done](done.md): whether the case would be served at once
- [select](../select.md), [timeout](../timeout.md)
- [sgcl::async::promise\<T\>](../promise.md)
