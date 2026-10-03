[sgcl](../../README.md) › [hash](../README.md) › [fnv64a](README.md)

# sgcl::hash::fnv64a::fnv64a

```cpp
fnv64a() noexcept = default;
```

Makes a hasher of no bytes yet, its state the offset basis, `0xcbf29ce484222325`, which [value](value.md) gives for
nothing. Go's `fnv.New64a()`. A hasher that goes on from a value saved earlier is made by [resume](resume.md); a
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
    hash::fnv64a h;
    println("{:016x}", h.value());
    h.update("foo");

    hash::fnv64a branch = h;
    branch.update("bar");
    println("{:016x}", branch.value());
    println("{} {}", h.value() == hash::fnv64a::of("foo"), branch.value() == hash::fnv64a::of("foobar"));
}
```

Output:

```text
cbf29ce484222325
85944171f73967e8
true true
```

## See also

- [resume](resume.md): a hasher going on from a saved value
- [sgcl::hash::fnv64a](README.md)
