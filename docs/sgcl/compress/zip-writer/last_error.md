[sgcl](../../README.md) › [compress](../README.md) › [zip](../zip.md) › [writer](../zip-writer.md)

# sgcl::compress::zip::writer::last_error

```cpp
const optional<error>& last_error() const noexcept;
```

Returns the first error the writer gave — a failure of `out`, or the caller's: data for a directory, an entry
`create` refuses, a create after the close, a comment too long — kept: every operation after it gave it at once and
wrote nothing. A write to an entry that was ended is not here: it fails by itself. A program that wants to react before the close looks here
after a `create`, or at the result of a single write.

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
    compress::zip::writer w(archive);
    io::writer logs = *w.create("logs/");
    (void)logs.write("data");  // a directory has none
    println("{}", w.last_error()->message());
    println("{}", w.add("c.txt", "c").has_value());
}
```

Output:

```text
zip: entry logs/: data for a directory
false
```

## See also

- [close](close.md): gives the same error
- [sgcl::compress::zip::writer](../zip-writer.md)
