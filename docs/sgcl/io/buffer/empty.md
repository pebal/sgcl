[sgcl](../../README.md) › [io](../README.md) › [buffer](../buffer.md)

# sgcl::io::buffer::empty

```cpp
bool empty() const noexcept;
```

Checks whether the buffer holds no bytes: `size() == 0`. A read of an empty buffer returns 0.

## Parameters

None.

## Return value

`true` when every byte written has been read, or none was written.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer queue("abc");
    vector<byte> one(1);
    while (!queue.empty()) {
        queue.read(one);
        println("{}", char(one[0]));
    }
}
```

Output:

```text
a
b
c
```

## See also

- [size](size.md)
- [sgcl::io::buffer](../buffer.md)
