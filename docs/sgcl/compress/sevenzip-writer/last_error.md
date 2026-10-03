[sgcl](../../README.md) › [compress](../README.md) › [sevenzip](../sevenzip.md) › [writer](README.md)

# sgcl::compress::sevenzip::writer::last_error

```cpp
const optional<error>& last_error() const noexcept;
```

Returns the first error the writer gave — a failure of the output, or the caller's: a name it refuses, data for a
directory, an entry after the close — kept: everything after it was refused. A write to an entry that was ended is
not here: it fails by itself. A
program that wants to react before the close looks here after a `create` or an `add`, or at the result of a single
write.

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
    io::buffer archive;
    compress::sevenzip::writer w(archive);
    w.add("a.txt", "a");
    println("{}", w.last_error().has_value());
    (void)w.close();
    w.add("b.txt", "b");
    println("{}", w.last_error()->io_error()->message());
}
```

Output:

```text
false
create 7z: stream closed
```

## See also

- [close](close.md): gives the same error
- [sgcl::compress::sevenzip::writer](README.md)
