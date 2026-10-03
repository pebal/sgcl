[sgcl](../../README.md) › [io](../README.md) › [buffer](README.md)

# sgcl::io::buffer::size

```cpp
size_t size() const noexcept;
```

Returns the number of bytes held: written and not yet read. It hides the `size` of
[mixin::seeker](../mixin/seeker/size.md), which would seek to the end and back to measure; the write position
plays no part.

## Parameters

None.

## Return value

The bytes held.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer data("12345678");
    vector<byte> three(3);
    data.read(three);
    data.seek(1);
    println("{} held, the position at {}", data.size(), *data.tell());
}
```

Output:

```text
5 held, the position at 1
```

## See also

- [empty](empty.md)
- [sgcl::io::buffer](README.md)
