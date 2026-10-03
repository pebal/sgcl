[sgcl](../../README.md) › [hash](../README.md) › [maphash](README.md)

# sgcl::hash::maphash::reset

```cpp
void reset() noexcept;
```

Puts the hasher back as it was made: no bytes, and the seed it was made with. Go's `h.Reset()`, which also keeps
the seed.

## Parameters

None.

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    hash::maphash h(42);
    h.update("first");
    h.reset();
    h.update("hello");
    println("{}", h.value() == hash::maphash::of("hello", 42));
}
```

Output:

```text
true
```

## See also

- [(constructor)](maphash.md): a hasher with the process's seed or a seed
- [sgcl::hash::maphash](README.md)
