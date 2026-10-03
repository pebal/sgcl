[sgcl](../../README.md) › [core](../README.md) › [generator](README.md)

# sgcl::generator\<T\>::value

```cpp
T& value() const noexcept;
```

The value of the last `co_yield`: a reference into the frame, to the value the promise keeps. It is overwritten by
the next `co_yield` and gone when the generator is destroyed. Precondition: the last [next](next.md) returned
`true`.

The reference is not `const`, so the consumer may take the value over with `std::move` (a `std::unique_ptr`, a
`string` without a copy); the frame then holds the moved-from object until the next `co_yield` replaces it. The
coroutine does not see the change: `co_yield` gave the promise its own copy.

## Parameters

None.

## Return value

A reference to the value.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

generator<string> words() {
    co_yield string("alpha");
    co_yield string("beta");
}

int main() {
    generator<string> g = words();
    vector<string> taken;
    while (g.next()) {
        taken.push_back(std::move(g.value()));  // the value taken over, not copied
    }
    println("{}", taken);
}
```

Output:

```text
["alpha", "beta"]
```

## See also

- [next](next.md): runs the coroutine to its next value
- [sgcl::generator\<T\>](README.md)
