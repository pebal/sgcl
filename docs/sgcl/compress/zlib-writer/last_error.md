[sgcl](../../README.md) › [compress](../README.md) › [zlib](../zlib.md) › [writer](../zlib-writer.md)

# sgcl::compress::zlib::writer::last_error

```cpp
const optional<io::error>& last_error() const noexcept;
```

Returns the first error the writer gave — a failure of `out`, a write or a flush after the close — kept: every
write, flush and close after it gave that error at once and wrote nothing. A program that wants to react before the
close looks here, or at the result of a single write. A [reset](reset.md) clears it.

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
    compress::zlib::writer w(sink);
    println("{}", w.last_error().has_value());
    (void)w.close();
    (void)w.write("too late");
    (void)w.write("and again");
    println("{}", w.last_error()->message());
}
```

Output:

```text
false
write zlib: stream closed
```

## See also

- [close](close.md): gives the same error
- [sgcl::compress::zlib::writer](../zlib-writer.md)
