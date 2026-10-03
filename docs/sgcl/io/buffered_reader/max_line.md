[sgcl](../../README.md) › [io](../README.md) › [buffered_reader](../buffered_reader.md)

# sgcl::io::buffered_reader::max_line

```cpp
size_t max_line() const noexcept;
```

Returns the longest line [read_line](read_line.md), [read_until](read_until.md) and [lines](lines.md) accept, in
bytes, as [set_max_line](set_max_line.md) set it; 0 for no bound, the default.

## Parameters

None.

## Return value

The bound, in bytes; 0 for none.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffered_reader in(io::stdin);
    println("{}", in.max_line());
    in.set_max_line(64 * 1024);
    println("{}", in.max_line());
}
```

Output:

```text
0
65536
```

## See also

- [set_max_line](set_max_line.md): sets the bound
- [sgcl::io::buffered_reader](../buffered_reader.md)
