[sgcl](../../README.md) › [compress](../README.md) › [sevenzip](../sevenzip.md) › [archive](README.md)

# sgcl::compress::sevenzip::archive::read, async_read

```cpp
expected<vector<byte>, error> read(const entry& e) const noexcept;                          // (1)
expected<vector<byte>, error> read(const entry& e, const limits& l) const;                  // (2)
expected<vector<byte>, error> read(const string& name) const noexcept;                      // (3)
expected<vector<byte>, error> read(const string& name, const limits& l) const;              // (4)
async::task<expected<vector<byte>, error>> async_read(const entry& e) const noexcept;       // (5)
async::task<expected<vector<byte>, error>> async_read(entry e, limits l) const noexcept;    // (6)
```

Reads an entry's data whole, decompressed and checked as its [reader](reader.md) checks it. The entry's size is held
against the limits' `max_size` before a byte is read; the vector is made at that size.

- (1), (3), (5) With the default [limits](../limits.md): 1 GiB.
- (2), (4), (6) With the limits given.
- (3–4) The first entry of the name.
- (5–6) Return a task: an archive in a file is decoded on the [blocking pool](../../async/spawn_blocking.md), the
  job holding the archive and the result; one in memory on the worker, which it lets go every 64 KB.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the entry |
| `name` | the entry's name |
| `l` | the bounds on the entry's size and the folder's decoders ([limits](../limits.md)) |

## Return value

The data, or the [error](../error/README.md): the entry past `max_size` (`errc::too_large`), no entry of the name
(`errc::invalid_argument`), the data's errors (`errc::password_required`, `errc::wrong_password`, `errc::checksum`,
`errc::corrupt`, `errc::unsupported`), a failure of the source (`errc::io`).

## Complexity

Linear in the size of the entry's folder up to the end of the entry.

## Exceptions

- (1), (3), (5), (6) None.
- (2), (4) `length_error` when the limits allow a size the archive gives past what a vector holds (`PTRDIFF_MAX`);
  none with a bound under it.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer archive;
    compress::sevenzip::writer w(archive);
    w.add("big.txt", string("x").repeat(1 << 20));
    (void)w.close();

    auto a = compress::sevenzip::archive::from(archive.data());
    println("{}", a->read("big.txt")->size());
    println("{}", a->read("big.txt", {.max_size = 1000}).error().message());
}
```

Output:

```text
1048576
7z: entry big.txt: larger than the limit
```

## See also

- [reader](reader.md), [walk](walk.md)
- [sgcl::compress::sevenzip::archive](README.md)
