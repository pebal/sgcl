[sgcl](../../README.md) › [hash](../README.md) › [crc64](README.md)

# sgcl::hash::crc64::reset

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
    hash::crc64 h;
    h.update("first");
    println("{:016x}", h.value());
    h.reset();
    h.update("second");
    println("{}", h.value() == hash::crc64::of("second"));
}
```

Output:

```text
f3e5067a2519ad56
true
```

## See also

- [(constructor)](crc64.md): a hasher as it is made
- [sgcl::hash::crc64](README.md)
