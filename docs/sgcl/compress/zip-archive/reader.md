[sgcl](../../README.md) › [compress](../README.md) › [zip](../zip.md) › [archive](../zip-archive.md)

# sgcl::compress::zip::archive::reader

```cpp
expected<io::reader, error> reader(const entry& e) const noexcept;        // (1)
expected<io::reader, error> reader(const string& name) const noexcept;    // (2)
```

Makes an [io reader](../../io/reader.md) of an entry's data, decompressed and checked: exactly the central record's
size, then 0; fewer or more bytes is `errc::corrupt`, a wrong CRC-32 `errc::checksum`, at the read that reaches the
end, and a failure names the entry as its path. Its first read reads the entry's local header to find where the
data starts; its name must be the central one. Readers of any number of entries may be open at once, from any
threads, each with a task form (`async_read`); the archive's source stays alive while one is.

1. The reader of the entry `e`, one of [entries](entries.md).
2. The reader of the first entry of the name.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the entry |
| `name` | the entry's name |

## Return value

The reader, or the [error](../error.md): (2) no entry of the name (`errc::invalid_argument`).

## Complexity

Constant; (2) linear in the number of entries.

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
    (void)w.add("log.txt", "GET /\nGET /about\n");
    (void)w.close();

    auto a = compress::zip::archive::from(archive.data());
    auto r = a->reader("log.txt");
    io::buffered_reader lines(*r);
    for (auto line : lines.lines()) {
        println("[{}]", line);
    }
    println("{}", a->reader("none.txt").error().message());
}
```

Output:

```text
[GET /]
[GET /about]
zip: no entry none.txt
```

## See also

- [read](read.md): the data whole
- [sgcl::compress::zip::archive](../zip-archive.md)
