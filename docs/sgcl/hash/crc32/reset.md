[sgcl](../../README.md) › [hash](../README.md) › [crc32](../crc32.md)

# sgcl::hash::crc32::reset

```cpp
void reset() noexcept;
```

Puts the hasher back as it was made: no bytes, and `value()` 0. A hasher made by [resume](resume.md) goes back to
that state too, not to the value it was resumed from. Go's `h.Reset()`.

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
    hash::crc32 h;
    h.update("first");
    println("{:08x}", h.value());
    h.reset();
    h.update("second");
    println("{}", h.value() == hash::crc32::of("second"));
}
```

Output:

```text
9271ee57
true
```

## See also

- [(constructor)](crc32.md): a hasher as it is made
- [sgcl::hash::crc32](../crc32.md)
