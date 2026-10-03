[sgcl](../../README.md) › [io](../README.md) › [buffer](README.md)

# sgcl::io::buffer::data

```cpp
slice<const byte> data() const noexcept;
```

Returns the bytes held, from the first not yet read to the end, as a [slice](../../core/slice/README.md), without taking
them: a read after it still reads them. The slice holds the memory it views, so it is never left dangling, but what
it shows is the buffer's bytes as they are until the next write, which may change them in place or move them.

## Parameters

None.

## Return value

A slice of the bytes held; empty for an empty buffer.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer in("GET / HTTP/1.1");
    slice<const byte> bytes = in.data();
    println("{} bytes, the first '{}'", bytes.size(), char(bytes[0]));

    vector<byte> verb(4);
    in.read(verb);
    println("{} bytes left, now first '{}'", in.data().size(), char(in.data()[0]));
}
```

Output:

```text
14 bytes, the first 'G'
10 bytes left, now first '/'
```

## See also

- [text](text.md): the same as a string
- [release](release.md): the bytes taken out
- [sgcl::io::buffer](README.md)
