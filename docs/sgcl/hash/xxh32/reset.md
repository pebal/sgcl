[sgcl](../../README.md) › [hash](../README.md) › [xxh32](README.md)

# sgcl::hash::xxh32::reset

```cpp
void reset() noexcept;
```

Puts the hasher back as it was made: no bytes, and the seed it was made with.

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
    hash::xxh32 h(42);
    h.update("first");
    h.reset();
    h.update("hello");
    println("{}", h.value() == hash::xxh32::of("hello", 42));
}
```

Output:

```text
true
```

## See also

- [(constructor)](xxh32.md): a hasher with a seed
- [sgcl::hash::xxh32](README.md)
