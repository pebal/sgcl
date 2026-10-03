[sgcl](../../README.md) › [hash](../README.md) › [adler32](../adler32.md)

# sgcl::hash::adler32::reset

```cpp
void reset() noexcept;
```

Puts the hasher back as it was made: `a` 1 and `b` 0, so that `value()` is 1. A hasher made by [resume](resume.md)
goes back to that state too, not to the value it was resumed from. Go's `h.Reset()`.

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
    hash::adler32 h;
    h.update("first");
    println("{:08x}", h.value());
    h.reset();
    h.update("second");
    println("{}", h.value() == hash::adler32::of("second"));
}
```

Output:

```text
06570229
true
```

## See also

- [(constructor)](adler32.md): a hasher as it is made
- [sgcl::hash::adler32](../adler32.md)
