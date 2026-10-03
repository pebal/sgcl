[sgcl](../../README.md) › [hash](../README.md) › [fnv64](../fnv64.md)

# sgcl::hash::fnv64::fnv64

```cpp
fnv64() noexcept = default;
```

Makes a hasher of no bytes yet, its state the offset basis, `0xcbf29ce484222325`, which [value](value.md) gives for
nothing. Go's `fnv.New64()`. A hasher that goes on from a value saved earlier is made by [resume](resume.md); a
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
    hash::fnv64 h;
    println("{:016x}", h.value());
    h.update("foo");

    hash::fnv64 branch = h;
    branch.update("bar");
    println("{:016x}", branch.value());
    println("{} {}", h.value() == hash::fnv64::of("foo"), branch.value() == hash::fnv64::of("foobar"));
}
```

Output:

```text
cbf29ce484222325
340d8765a4dda9c2
true true
```

## See also

- [resume](resume.md): a hasher going on from a saved value
- [sgcl::hash::fnv64](../fnv64.md)
