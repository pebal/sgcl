[sgcl](../../README.md) › [compress](../README.md) › [lzma](../lzma.md) › [writer](../lzma-writer.md)

# sgcl::compress::lzma::writer::last_error

```cpp
const optional<io::error>& last_error() const noexcept;
```

Returns the first error the writer gave — a failure of `out`, options out of range, a write after the close — kept:
every write and close after it gave that error at once and wrote nothing. A [reset](reset.md) clears it.

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
    compress::lzma::writer w(sink, {.level = compress::level::huffman_only});
    (void)w.write("first");
    (void)w.write("second");
    println("{}", w.last_error()->message());
    println("{}", w.close().error().message());
}
```

Output:

```text
write lzma: invalid argument
write lzma: invalid argument
```

## See also

- [close](close.md): gives the same error
- [sgcl::compress::lzma::writer](../lzma-writer.md)
