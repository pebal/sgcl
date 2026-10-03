[sgcl](../../README.md) › [compress](../README.md) › [flate](../flate/README.md) › [writer](README.md)

# sgcl::compress::flate::writer::last_error

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
    compress::flate::writer w(sink);
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
write flate: stream closed
```

## See also

- [close](close.md): gives the same error
- [sgcl::compress::flate::writer](README.md)
