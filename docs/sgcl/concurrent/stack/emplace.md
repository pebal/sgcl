[sgcl](../../README.md) › [concurrent](../README.md) › [stack](../stack.md)

# sgcl::concurrent::stack\<T\>::emplace

```cpp
template<class... A>
void emplace(A&&... a) noexcept(std::is_nothrow_constructible_v<T, A...>);
```

Puts an element constructed in place from `a...` on the top: `T(std::forward<A>(a)...)` in a new node on the
managed heap, published on the head as [push](push.md) publishes it.

## Parameters

| Parameter | Description |
|---|---|
| `a` | the arguments the element is constructed from |

## Return value

None.

## Complexity

Constant, plus the retries of a lost compare-exchange when other threads push or pop at once.

## Exceptions

What the constructor of `T` throws; none when it is noexcept.

If an exception is thrown, nothing is linked and the stack is as it was.

## Notes

Lock-free, and linearizable at the compare-exchange on the head, with the backoff after a lost exchange, as
`push` is; it wakes a thread waiting in [pop](pop.md) when there is one. `push` is this function with the value
as its argument.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

struct Frame {
    string function;
    int line;
};

int main() {
    concurrent::stack<Frame> calls;
    calls.emplace("main", 12);
    calls.emplace("parse", 40);

    while (auto frame = calls.try_pop()) {
        println("{} {}", frame->function, frame->line);
    }
}
```

Output:

```text
parse 40
main 12
```

## See also

- [push](push.md): puts a copy or a moved value
- [sgcl::concurrent::stack\<T\>](../stack.md)
