[sgcl](../../README.md) › [compress](../README.md) › [sevenzip](../sevenzip.md) › [archive](../sevenzip-archive.md)

# sgcl::compress::sevenzip::archive::from

```cpp
static expected<archive, error> from(const slice<const byte>& data) noexcept;    // (1)
static expected<archive, error> from(const slice<const byte>& data,              // (2)
                                     const limits& l) noexcept;
static expected<archive, error> from(const slice<const byte>& data,              // (3)
                                     const options& o) noexcept;
```

Opens an archive in memory, as [open](open.md) opens one of a file. A slice of a managed buffer (a `vector<byte>`,
an `io::buffer`'s data) keeps the buffer alive while the archive is used; a buffer of unmanaged memory is the
caller's to keep. An archive in memory is decoded on the thread or the worker that reads it.

1. With the default [limits](../limits.md) and no password.
2. With the limits given.
3. With the password and the limits of the [options](../sevenzip-options.md).

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes of the archive |
| `l` | the bounds of the header and of the reads |
| `o` | the password and the limits |

## Return value

The archive, or the [error](../error.md) as [open](open.md) gives it.

## Complexity

Linear in the size of the header.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    compress::sevenzip::entry_info info;  // a fixed time: the same bytes every run
    info.modified = time::datetime::from_unix(1790000000, time::zone::utc());
    io::buffer upload;
    compress::sevenzip::writer w(upload);
    w.add("a.txt", "first\n", info);
    w.add("b.txt", "second\n", info);
    (void)w.close();

    auto a = compress::sevenzip::archive::from(upload.data());
    println("{} entries", a->entries().size());
    auto few = compress::sevenzip::archive::from(upload.data(), {.max_entries = 1});
    println("{}", few.error().message());
}
```

Output:

```text
2 entries
offset 154: 7z: more entries than the limit allows
```

## See also

- [open](open.md)
- [sgcl::compress::sevenzip::archive](../sevenzip-archive.md)
