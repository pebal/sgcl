[sgcl](../../README.md) › [hash](../README.md) › [xxh3_128](../xxh3_128.md)

# sgcl::hash::xxh3_128::reset

```cpp
void reset() noexcept;
```

Puts the hasher back as it was made: no bytes, and the seed it was made with. Go's `h.Reset()` in zeebo/xxh3.

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
    hash::xxh3_128 h(42);
    h.update("first");
    h.reset();
    h.update("hello");
    println("{}", h.value() == hash::xxh3_128::of("hello", 42));
}
```

Output:

```text
true
```

## See also

- [(constructor)](xxh3_128.md): a hasher with a seed
- [sgcl::hash::xxh3_128](../xxh3_128.md)
