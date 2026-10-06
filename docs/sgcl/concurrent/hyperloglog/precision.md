[sgcl](../../README.md) › [concurrent](../README.md) › [hyperloglog](README.md)

# sgcl::concurrent::hyperloglog::precision

```cpp
unsigned precision() const noexcept;
```

Returns the precision, *p*: the sketch has 2^*p* registers.

## Parameters

None.

## Return value

The precision.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/concurrent.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    concurrent::hyperloglog h;
    println("{}", h.precision());
}
```

Output:

```text
14
```

## See also

- [(constructor)](hyperloglog.md)
- [sgcl::concurrent::hyperloglog](README.md)
