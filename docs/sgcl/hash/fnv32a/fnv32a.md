[sgcl](../../README.md) › [hash](../README.md) › [fnv32a](../fnv32a.md)

# sgcl::hash::fnv32a::fnv32a

```cpp
fnv32a() noexcept = default;
```

Makes a hasher of no bytes yet, its state the offset basis, `0x811c9dc5`, which [value](value.md) gives for
nothing. Go's `fnv.New32a()`. A hasher that goes on from a value saved earlier is made by [resume](resume.md); a
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
    hash::fnv32a h;
    println("{:08x}", h.value());
    h.update("foo");

    hash::fnv32a branch = h;
    branch.update("bar");
    println("{:08x}", branch.value());
    println("{} {}", h.value() == hash::fnv32a::of("foo"), branch.value() == hash::fnv32a::of("foobar"));
}
```

Output:

```text
811c9dc5
bf9cf968
true true
```

## See also

- [resume](resume.md): a hasher going on from a saved value
- [sgcl::hash::fnv32a](../fnv32a.md)
