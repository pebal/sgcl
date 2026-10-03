[sgcl](../../README.md) › [io](../README.md) › [buffer](README.md)

# sgcl::io::buffer::release

```cpp
vector<byte> release() const noexcept;
```

Takes the bytes held out, as a vector of their own, and leaves the buffer empty, its write position at the end. The
bytes already read are not in the vector. What a function that built its output in a buffer returns.

## Parameters

None.

## Return value

The bytes held, from the first not yet read to the end.

## Complexity

Linear in the bytes held: they are copied into a vector of their size.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

vector<byte> packet(const string& payload) {
    io::buffer out;
    out.write(byte(payload.size()));
    out.write(payload);
    return out.release();
}

int main() {
    vector<byte> p = packet("ping");
    println("{} bytes, the length byte {}", p.size(), int(p[0]));
}
```

Output:

```text
5 bytes, the length byte 4
```

## See also

- [data](data.md): the bytes held, as a view
- [read_all](../mixin/reader/read_all.md): the bytes read out as a stream's
- [sgcl::io::buffer](README.md)
