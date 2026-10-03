[sgcl](../../README.md) › [io](../README.md) › [buffered_reader](../buffered_reader.md)

# sgcl::io::operator== (sgcl::io::buffered_reader)

```cpp
friend bool operator==(const buffered_reader& a, const buffered_reader& b) noexcept;
```

Checks whether `a` and `b` are the same reader: handles of one state, a copy of the other. Two readers made over the
same stream are two readers, with two blocks and two positions, and compare unequal. A hidden friend, found by the
arguments' type alone; `a != b` is rewritten to it by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when both hold the same reader, or both hold none; `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer source("text");
    io::buffered_reader a(source);
    io::buffered_reader copy = a;
    io::buffered_reader b(source);
    println("{} {}", a == copy, a == b);
}
```

Output:

```text
true false
```

## See also

- [operator bool](operator_bool.md): checks whether the handle holds a reader
- [sgcl::io::buffered_reader](../buffered_reader.md)
