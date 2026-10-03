[sgcl](../../README.md) › [compress](../README.md) › [sevenzip](../sevenzip.md) › [archive](README.md)

# sgcl::compress::sevenzip::archive::reader

```cpp
expected<io::reader, error> reader(const entry& e) const noexcept;                     // (1)
expected<io::reader, error> reader(const entry& e, const limits& l) const noexcept;    // (2)
expected<io::reader, error> reader(const string& name) const noexcept;                 // (3)
```

Makes an [io reader](../../io/reader/README.md) of an entry's data, decompressed and checked: its folder decoded from the
start and what comes before the entry dropped, the data counted to the entry's size and checked against its CRC-32.
Readers of any entries may be open at once, from any threads; reading every entry of a solid folder this way costs
the square of the folder's size, which [walk](walk.md) does not. An entry without data gives a reader of nothing.

1. With the archive's limits.
2. With the limits given: `max_memory` bounds the folder's decoders.
3. The first entry of the name.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the entry |
| `l` | the bound on the folder's decoders ([limits](../limits.md)) |
| `name` | the entry's name |

## Return value

The reader, or the [error](../error/README.md): (3) no entry of the name (`errc::invalid_argument`). The errors of the data
— a password missing or wrong, a damaged folder, a CRC-32 that does not match — are the reader's reads'.

## Complexity

Constant; (3) linear in the number of entries.

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
    w.add("log.txt", "GET /\nGET /about\n");
    (void)w.close();

    auto a = compress::sevenzip::archive::from(archive.data());
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
7z: no entry none.txt
```

## See also

- [read](read.md): the data whole; [walk](walk.md): every entry in order
- [sgcl::compress::sevenzip::archive](README.md)
