[sgcl](../../README.md) › [compress](../README.md) › [zstd](../zstd/README.md) › [writer](README.md)

# sgcl::compress::zstd::writer::is_closed

```cpp
bool is_closed() const noexcept;
```

Checks whether the stream was closed: whether [close](close.md) ran past its first checks. A [reset](reset.md)
opens a new stream.

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
    compress::zstd::writer w(sink);
    println("{}", w.is_closed());
    (void)w.close();
    println("{}", w.is_closed());
}
```

Output:

```text
false
true
```

## See also

- [close](close.md)
- [sgcl::compress::zstd::writer](README.md)
