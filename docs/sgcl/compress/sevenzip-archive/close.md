[sgcl](../../README.md) › [compress](../README.md) › [sevenzip](../sevenzip.md) › [archive](README.md)

# sgcl::compress::sevenzip::archive::close

```cpp
expected<void, error> close() noexcept;
```

Closes the file the archive opened itself from a path ([open](open.md) of a path); a file given to `open` is the
caller's to close, and an archive in memory has nothing to close.

## Parameters

None.

## Return value

Nothing, or the [error](../error/README.md) of the file's close (`errc::io`).

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
    {
        compress::sevenzip::writer w("docs.7z");
        w.add("a.txt", "a");
        (void)w.close();
    }
    auto a = compress::sevenzip::archive::open("docs.7z");
    println("{}", a->entries()[0].name);
    println("{}", a->close().has_value());
    (void)io::remove("docs.7z");
}
```

Output:

```text
a.txt
true
```

## See also

- [open](open.md)
- [sgcl::compress::sevenzip::archive](README.md)
