[sgcl](../../README.md) › [io](../README.md) › [buffer](README.md)

# sgcl::io::buffer::clear

```cpp
void clear() const noexcept;
```

Drops every byte held and puts the write position back at the end. The memory is kept for the writes to come, as
`std::vector::clear` keeps it: a buffer cleared and filled again in a loop allocates once.

## Parameters

None.

## Return value

None.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer pending("unsent data");
    pending.seek(2);
    println("{} bytes pending", pending.size());
    pending.clear();
    println("{} bytes pending, the position at {}", pending.size(), *pending.tell());
    pending.write("new");
    println("{}", pending.text());
}
```

Output:

```text
11 bytes pending
0 bytes pending, the position at 0
new
```

## See also

- [release](release.md): the bytes taken out
- [sgcl::io::buffer](README.md)
