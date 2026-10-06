[sgcl](../../README.md) › [compress](../README.md) › [lz4](../lz4/README.md) › [writer](README.md)

# sgcl::compress::lz4::writer::last_error

```cpp
const optional<io::error>& last_error() const noexcept;
```

Returns the first error the writer gave — a failure of `out`, options out of range, a write after the close — kept: every write,
flush and close after it gave that error at once and wrote nothing. A [reset](reset.md) clears it.

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
    compress::lz4::writer w(sink, {.block_size = compress::lz4::block_size(9)});
    (void)w.write("first");
    (void)w.write("second");
    println("{}", w.last_error()->message());
    println("{}", w.close().error().message());
}
```

Output:

```text
write lz4: a block size of kb64, kb256, mb1 or mb4: invalid argument
write lz4: a block size of kb64, kb256, mb1 or mb4: invalid argument
```

## See also

- [close](close.md): gives the same error
- [sgcl::compress::lz4::writer](README.md)
