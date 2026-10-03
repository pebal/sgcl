[sgcl](../../README.md) › [core](../README.md) › [generator](../generator.md)

# sgcl::generator\<T\>::end

```cpp
iterator end() noexcept;
```

The end iterator: an [iterator](../generator-iterator.md) that refers to no generator, which an iterator of this
generator equals once the coroutine has ended. It does not run the coroutine.

## Parameters

None.

## Return value

The end iterator.

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

- [begin](begin.md): runs the coroutine to its first value
- [sgcl::generator\<T\>](../generator.md)
