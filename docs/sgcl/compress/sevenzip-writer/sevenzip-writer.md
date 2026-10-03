[sgcl](../../README.md) › [compress](../README.md) › [sevenzip](../sevenzip.md) › [writer](../sevenzip-writer.md)

# sgcl::compress::sevenzip::writer::writer

```cpp
/*(1)*/ explicit writer(const string& path) noexcept;
/*(2)*/ writer(const string& path, const options& o) noexcept;
/*(3)*/ explicit writer(const io::file& file) noexcept;
/*(4)*/ writer(const io::file& file, const options& o) noexcept;
/*(5)*/ explicit writer(const io::buffer& b) noexcept;
/*(6)*/ writer(const io::buffer& b, const options& o) noexcept;
```

Constructs a writer of an archive. Nothing of the archive is written yet; with a password in the options, its key is
made here (about 12 ms), and the password's bytes are not read after.

- (1–2) Into a file made at the path, which [close](close.md) closes; failing to make it is the writer's first
  error.
- (3–4) Into a file from where it stands; the file is the caller's to close.
- (5–6) Into a buffer from its write position; the writer holds the buffer, a handle: the same buffer as the
  caller's.
- (1), (3), (5) With the default [options](../sevenzip-options.md): LZMA2 at level 6, solid, automatic filters.

## Parameters

| Parameter | Description |
|---|---|
| `path` | where the archive is made |
| `file` | an open file the archive is written into |
| `b` | a buffer the archive is written into |
| `o` | how the archive is written ([options](../sevenzip-options.md)) |

## Complexity

Constant; with a password, the key's 2^19 rounds.

## Exceptions

None.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    compress::sevenzip::writer w("no-such-directory/a.7z");
    w.add("a.txt", "a");
    println("{}", w.close().error().message());

    io::buffer archive;
    compress::sevenzip::writer b(archive, {.method = compress::sevenzip::method::copy});
    b.add("a.txt", "a");
    println("{}", b.close().has_value());
}
```

Output:

```text
input/output error: open no-such-directory/a.7z: No such file or directory
true
```

## See also

- [options](../sevenzip-options.md)
- [sgcl::compress::sevenzip::writer](../sevenzip-writer.md)
