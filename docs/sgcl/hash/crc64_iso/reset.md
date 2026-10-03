[sgcl](../../README.md) › [hash](../README.md) › [crc64_iso](README.md)

# sgcl::hash::crc64_iso::reset

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
    hash::crc64_iso h;
    h.update("first");
    println("{:016x}", h.value());
    h.reset();
    h.update("second");
    println("{}", h.value() == hash::crc64_iso::of("second"));
}
```

Output:

```text
287c6dc990300000
true
```

## See also

- [(constructor)](crc64_iso.md): a hasher as it is made
- [sgcl::hash::crc64_iso](README.md)
