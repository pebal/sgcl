[sgcl](../../README.md) › [compress](../README.md) › [zip](../zip.md) › [archive](../zip-archive.md)

# sgcl::compress::zip::archive::close

```cpp
expected<void, error> close() noexcept;
```

Closes the file the archive opened itself from a path ([open](open.md) of a path); a file given to `open` is the
caller's to close, and an archive in memory has nothing to close.

## Parameters

None.

## Return value

Nothing, or the [error](../error.md) of the file's close (`errc::io`).

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
    (void)io::mkdir_all("docs");
    (void)io::write_file("docs/a.txt", "a");
    (void)compress::zip::create("docs", "docs.zip");

    auto a = compress::zip::archive::open("docs.zip");
    println("{}", a->entries()[0].name);
    println("{}", a->close().has_value());
    (void)io::remove_all("docs");
    (void)io::remove("docs.zip");
}
```

Output:

```text
a.txt
true
```

## See also

- [open](open.md)
- [sgcl::compress::zip::archive](../zip-archive.md)
