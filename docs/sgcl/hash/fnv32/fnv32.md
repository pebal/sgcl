[sgcl](../../README.md) › [hash](../README.md) › [fnv32](../fnv32.md)

# sgcl::hash::fnv32::fnv32

```cpp
fnv32() noexcept = default;
```

Makes a hasher of no bytes yet, its state the offset basis, `0x811c9dc5`, which [value](value.md) gives for
nothing. Go's `fnv.New32()`. A hasher that goes on from a value saved earlier is made by [resume](resume.md); a
copy is a branch, a hasher that goes on from the same bytes on its own.

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
    hash::fnv32 h;
    println("{:08x}", h.value());
    h.update("foo");

    hash::fnv32 branch = h;
    branch.update("bar");
    println("{:08x}", branch.value());
    println("{} {}", h.value() == hash::fnv32::of("foo"), branch.value() == hash::fnv32::of("foobar"));
}
```

Output:

```text
811c9dc5
31f0b262
true true
```

## See also

- [resume](resume.md): a hasher going on from a saved value
- [sgcl::hash::fnv32](../fnv32.md)
