[sgcl](../../README.md) › [hash](../README.md) › [siphash](README.md)

# sgcl::hash::siphash::reset

```cpp
void reset() noexcept;
```

Puts the hasher back as it was made: no bytes, and the same key. Go's `h.Reset()`.

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
    array<byte, 16> key = {};
    hash::siphash h(key);
    h.update("first");
    h.reset();
    h.update("hello");
    println("{}", h.value() == hash::siphash::of("hello", key));
}
```

Output:

```text
true
```

## See also

- [(constructor)](siphash.md): a hasher under a key
- [sgcl::hash::siphash](README.md)
