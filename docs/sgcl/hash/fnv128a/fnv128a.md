[sgcl](../../README.md) › [hash](../README.md) › [fnv128a](README.md)

# sgcl::hash::fnv128a::fnv128a

```cpp
fnv128a() noexcept = default;
```

Makes a hasher of no bytes yet, its state the offset basis, `0x6c62272e07bb014262b821756295c58d`, which
[value](value.md) gives for nothing. Go's `fnv.New128a()`. A hasher that goes on from a value saved earlier is made
by [resume](resume.md); a copy is a branch, a hasher that goes on from the same bytes on its own.

## Parameters

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    hash::fnv128a h;
    println("{}", encoding::hex::encode(h.value()));
    h.update("foo");

    hash::fnv128a branch = h;
    branch.update("bar");
    println("{}", encoding::hex::encode(branch.value()));
    println("{} {}", h.value() == hash::fnv128a::of("foo"), branch.value() == hash::fnv128a::of("foobar"));
}
```

Output:

```text
6c62272e07bb014262b821756295c58d
343e1662793c64bf6f0d3597ba446f18
true true
```

## See also

- [resume](resume.md): a hasher going on from a saved value
- [sgcl::hash::fnv128a](README.md)
