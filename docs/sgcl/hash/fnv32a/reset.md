[sgcl](../../README.md) › [hash](../README.md) › [fnv32a](README.md)

# sgcl::hash::fnv32a::reset

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
    hash::fnv32a h;
    h.update("first");
    println("{:08x}", h.value());
    h.reset();
    h.update("second");
    println("{}", h.value() == hash::fnv32a::of("second"));
}
```

Output:

```text
4881d841
true
```

## See also

- [(constructor)](fnv32a.md): a hasher as it is made
- [sgcl::hash::fnv32a](README.md)
