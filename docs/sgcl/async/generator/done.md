[sgcl](../../README.md) › [async](../README.md) › [generator](../generator.md)

# sgcl::async::generator\<T\>::done

```cpp
bool done() const noexcept;
```

Checks whether the coroutine has ended, at the end of its body, by a `co_return;` or by an exception; an empty
generator is done. It does not run the coroutine: a generator that has yielded its last value is not done until a
`next()` finds its end.

## Parameters

None.

## Return value

`true` when the coroutine has ended or the generator is empty, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::generator<int> one() {
    co_yield 1;
}

async::task<> consume() {
    auto g = one();
    println("{}", g.done());
    println("{}", *co_await g.next());
    println("{}", g.done());  // suspended at the co_yield
    println("{}", (co_await g.next()).has_value());
    println("{}", g.done());
}

int main() {
    consume().wait();
}
```

Output:

```text
false
1
false
false
true
```

## See also

- [next](next.md): runs the coroutine to its next value
- [sgcl::async::generator\<T\>](../generator.md)
