[sgcl](../../README.md) › [hash](../README.md) › [adler32](README.md)

# sgcl::hash::adler32::adler32

```cpp
adler32() noexcept = default;
```

Makes a hasher of no bytes yet: `a` is 1 and `b` is 0, so that [value](value.md) is 1, the checksum of nothing.
Go's `adler32.New()`. A hasher that goes on from a checksum saved earlier is made by [resume](resume.md); a copy is
a branch, a hasher that goes on from the same bytes on its own.

## Parameters

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
    hash::adler32 h;
    println("{:08x}", h.value());
    h.update("Wikipedia");
    println("{:08x}", h.value());

    hash::adler32 branch = h;
    branch.update("!");
    println("{} {}", h.value() == hash::adler32::of("Wikipedia"), branch.value() == hash::adler32::of("Wikipedia!"));
}
```

Output:

```text
00000001
11e60398
true true
```

## See also

- [resume](resume.md): a hasher going on from a saved checksum
- [sgcl::hash::adler32](README.md)
