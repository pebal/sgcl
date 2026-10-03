[sgcl](../../README.md) › [compress](../README.md) › [tar](../tar.md) › [writer](../tar-writer.md)

# sgcl::compress::tar::writer::last_error

```cpp
const optional<error>& last_error() const noexcept;
```

Returns the first error the writer gave, kept — a failure of `out`, a header or a write it could not take, a call
after the close — as the archive's [error](../error.md), with the entry it was in and no place, as it did not come
from data read (its message is the words alone). Every
`write_header`, `write` and `close` after it gave that error at once and wrote nothing. A program that wants to react
before the close looks here, or at the result of `write_header` or of a single `write`.

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
    compress::tar::writer w(archive);
    (void)w.write_header({.name = "a.txt", .size = 3});
    (void)w.write_header({.name = "b.txt", .size = 3});  // a.txt's data not written
    (void)w.write("abc");
    println("{}", w.last_error()->message());
    println("{}", w.close().has_value());
}
```

Output:

```text
tar: entry a.txt: 3 bytes of its data not written
false
```

## See also

- [close](close.md): gives the same error
- [sgcl::compress::tar::writer](../tar-writer.md)
