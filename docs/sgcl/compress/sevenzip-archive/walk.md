[sgcl](../../README.md) › [compress](../README.md) › [sevenzip](../sevenzip.md) › [archive](../sevenzip-archive.md)

# sgcl::compress::sevenzip::archive::walk, async_walk

```cpp
/*(1)*/ generator<pair<entry, io::reader>> walk() const noexcept;
/*(2)*/ generator<pair<entry, io::reader>> walk(const limits& l) const noexcept;
/*(3)*/ async::generator<pair<entry, io::reader>> async_walk() const noexcept;
/*(4)*/ async::generator<pair<entry, io::reader>> async_walk(const limits& l) const noexcept;
```

Gives every entry in the archive's order, each with a reader of its data, decoding each folder once: `for (auto
[e, r] : a.walk())`. A reader serves until the walk goes on, which decodes and drops what the reader has not read of
its entry and makes the reader `io::errc::closed`. An entry the walk cannot read (encrypted with no password or a
wrong one, an unsupported method, a damaged folder) has a reader that fails, and the walk goes on to the next.

1. A [generator](../../core/generator.md) for a thread, with the archive's limits.
2. With the limits given.
3. The same for a task, an [async::generator](../../async/generator.md): `while (auto v = co_await g.next())`.
4. With the limits given.

## Parameters

| Parameter | Description |
|---|---|
| `l` | the bound on a folder's decoders ([limits](../limits.md)) |

## Return value

The generator of the entries and their readers.

## Complexity

Linear in the size of the archive's data.

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
    w.add_directory("docs");
    w.add("docs/notes.txt", "remember the milk\n");
    w.add("readme.md", "# readme\n");
    (void)w.close();

    auto a = compress::sevenzip::archive::from(archive.data());
    for (auto [e, r] : a->walk()) {  // one decoder a folder
        if (e.is_directory || e.is_anti) {
            println("{}/", e.name);
            continue;
        }
        auto data = r.read_all();  // checked against the entry's CRC-32
        println("{} {} bytes{}", e.name, data->size(), e.is_local() ? "" : " (not a local name)");
    }
}
```

Output:

```text
docs/
docs/notes.txt 18 bytes
readme.md 9 bytes
```

## See also

- [reader](reader.md): one entry
- [sgcl::compress::sevenzip::archive](../sevenzip-archive.md)
