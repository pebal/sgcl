[sgcl](../../README.md) › [io](../README.md) › [buffered_writer](../buffered_writer.md)

# sgcl::io::operator== (sgcl::io::buffered_writer)

```cpp
friend bool operator==(const buffered_writer& a, const buffered_writer& b) noexcept;
```

Checks whether `a` and `b` are the same writer: handles of one state, a copy of the other. Two writers made over the
same stream are two writers, with two blocks, and compare unequal. A hidden friend, found by the arguments' type
alone; `a != b` is rewritten to it by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when both hold the same writer, or both hold none; `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffered_writer a(io::stdout);
    io::buffered_writer copy = a;
    io::buffered_writer b(io::stdout);
    println("{} {}", a == copy, a == b);
}
```

Output:

```text
true false
```

## See also

- [operator bool](operator_bool.md): checks whether the handle holds a writer
- [sgcl::io::buffered_writer](../buffered_writer.md)
