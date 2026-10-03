[sgcl](../../README.md) › [compress](../README.md) › [lzw](../lzw.md) › [writer](../lzw-writer.md)

# sgcl::compress::lzw::writer::last_error

```cpp
const optional<io::error>& last_error() const noexcept;
```

Returns the first error the writer gave — a failure of `out`, a byte the literal width cannot hold, a write after the
close — kept: every write and close after it gave that error at once and wrote nothing. A [reset](reset.md) clears it.

## Parameters

None.

## Return value

The first error, or `nullopt` while there was none.

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
    compress::lzw::writer w(sink, compress::lzw::order::lsb, 7);
    (void)w.write("café");  // é is past 7 bits
    (void)w.write("more");
    println("{}", w.last_error()->message());
    println("{}", w.close().error().message());
}
```

Output:

```text
write lzw: invalid argument
write lzw: invalid argument
```

## See also

- [close](close.md): gives the same error
- [sgcl::compress::lzw::writer](../lzw-writer.md)
