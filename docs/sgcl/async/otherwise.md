[sgcl](../README.md) › [async](README.md)

# sgcl::async::otherwise

```cpp
#include "sgcl/async/select.h"   // or "sgcl/async.h"

namespace sgcl::async {
    template<class F>
    class otherwise_case;

    template<class F>
    otherwise_case<F> otherwise(F f) noexcept(std::is_nothrow_move_constructible_v<F>);
}
```

The case of a [select](select.md) served when no other case can be served at once: Go's `default`. A select with it
never waits: it tries every other case once, from a random one on, and when none is ready it runs `f()` and gives the
index of the `otherwise`. That is how a poll is written: a receive, a send or a lock taken if it can be at once, and
something else done if not. A select has at most one `otherwise`, wherever it stands among the cases.

`otherwise_case<F>` is the case: it holds the body and is passed to the select by value.

## Parameters

| Parameter | Description |
|---|---|
| `f` | the body of the case, `void()` |

## Return value

The case, `otherwise_case<F>`, holding `f`.

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

void poll(async::channel<int> numbers) {
    async::select(
        numbers.on_receive([](int n) { println("got {}", n); }),
        async::otherwise([] { println("nothing yet"); })).wait();  // never waits
}

int main() {
    async::channel<int> numbers(4);
    poll(numbers);
    numbers.send(42).wait();
    poll(numbers);
    poll(numbers);
}
```

Output:

```text
nothing yet
got 42
nothing yet
```

## See also

- [select](select.md): the wait over the cases
- [try_receive](channel/try_receive.md), [try_send](channel/try_send.md): a channel's own forms that never wait
