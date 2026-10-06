[sgcl](../../README.md) › [encoding](../README.md) › [uuid](README.md)

# sgcl::encoding::uuid::to_string

```cpp
string to_string() const noexcept;
```

The text, `8-4-4-4-12` in lower-case hexadecimal, 36 characters, as RFC 9562 writes it and Go's and Python's
write it; what `println("{}", id)` writes too.

## Parameters

None.

## Return value

The text.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::uuid id("{F81D4FAE-7DEC-11D0-A765-00A0C91E6BF6}");
    println(id.to_string());
    println("[{:>38}]", id);
}
```

Output:

```text
f81d4fae-7dec-11d0-a765-00a0c91e6bf6
[  f81d4fae-7dec-11d0-a765-00a0c91e6bf6]
```

## See also

- [parse](parse.md): the other way
- [sgcl::encoding::uuid](README.md)
