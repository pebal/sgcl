[sgcl](../../README.md) › [core](../README.md) › [generator](README.md)

# sgcl::generator\<T\>::end

```cpp
std::default_sentinel_t end() const noexcept;
```

The end of a range-for over the generator: `std::default_sentinel`, which an [iterator](../generator-iterator.md)
of this generator compares equal to once the coroutine has ended. The comparison is a look: an iterator that holds
no value yet runs the coroutine to its next `co_yield` to tell whether it has ended. `end` itself runs nothing.

## Parameters

None.

## Return value

`std::default_sentinel`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"

using namespace sgcl;

generator<int> nothing() {
    co_return;
}

int main() {
    generator<int> g = nothing();
    println("{}", g.begin() == g.end());
}
```

Output:

```text
true
```

## See also

- [begin](begin.md): the iterator, which runs the coroutine at its first look
- [sgcl::generator\<T\>](README.md)
