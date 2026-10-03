[sgcl](../../README.md) › [hash](../README.md) › [fnv32](README.md)

# sgcl::hash::fnv32::reset

```cpp
void reset() noexcept;
```

Puts the hasher back as it was made: the offset basis, `0x811c9dc5`. A hasher made by [resume](resume.md) goes back
to it too, not to the value it was resumed from. Go's `h.Reset()`.

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
    hash::fnv32 h;
    h.update("first");
    println("{:08x}", h.value());
    h.reset();
    h.update("second");
    println("{}", h.value() == hash::fnv32::of("second"));
}
```

Output:

```text
3b83da79
true
```

## See also

- [(constructor)](fnv32.md): a hasher as it is made
- [sgcl::hash::fnv32](README.md)
