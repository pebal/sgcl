[sgcl](../../README.md) › [compress](../README.md) › [bzip2](../bzip2/README.md) › [writer](README.md)

# sgcl::compress::bzip2::writer::last_error

```cpp
const optional<io::error>& last_error() const noexcept;
```

Returns the first error the writer gave — a failure of `out`, a write after the close — kept: every write,
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
    compress::bzip2::writer w(sink);
    (void)w.close();
    (void)w.write("late");
    println("{}", w.last_error()->message());
    println("{}", w.close().error().message());
}
```

Output:

```text
write bzip2: stream closed
write bzip2: stream closed
```

## See also

- [close](close.md): gives the same error
- [sgcl::compress::bzip2::writer](README.md)
