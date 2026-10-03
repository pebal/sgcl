[sgcl](../../README.md) › [compress](../README.md) › [gzip](../gzip/README.md) › [writer](README.md)

# sgcl::compress::gzip::writer::is_closed

```cpp
bool is_closed() const noexcept;
```

Checks whether the stream was closed: whether [close](close.md) ran, whatever it gave. A [reset](reset.md) opens a
new stream.

## Parameters

None.

## Return value

`true` after a close, `false` before it and after a reset.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer sink;
    compress::gzip::writer w(sink);
    println("{}", w.is_closed());
    (void)w.close();
    println("{}", w.is_closed());
    w.reset(io::buffer());
    println("{}", w.is_closed());
}
```

Output:

```text
false
true
false
```

## See also

- [close](close.md)
- [sgcl::compress::gzip::writer](README.md)
