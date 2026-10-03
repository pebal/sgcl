[sgcl](../../README.md) › [io](../README.md) › [buffered_reader](README.md)

# sgcl::io::buffered_reader::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the handle holds a reader. It holds none when default-constructed, and every operation on it is then a
contract violation; a reader made over a stream is held from its constructor on.

## Parameters

None.

## Return value

`true` when the handle holds a reader, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffered_reader in;
    println("{}", bool(in));
    in = io::buffered_reader(io::stdin);
    println("{}", bool(in));
}
```

Output:

```text
false
true
```

## See also

- [(constructor)](buffered_reader.md): makes a reader, or an empty handle
- [sgcl::io::buffered_reader](README.md)
