[sgcl](../../README.md) › [compress](../README.md) › [zip](../zip.md) › [archive](../zip-archive.md)

# sgcl::compress::zip::archive::read, async_read

```cpp
/*(1)*/ expected<vector<byte>, error> read(const entry& e) const noexcept;
/*(2)*/ expected<vector<byte>, error> read(const entry& e, const limits& l) const;
/*(3)*/ expected<vector<byte>, error> read(const string& name) const noexcept;
/*(4)*/ expected<vector<byte>, error> read(const string& name, const limits& l) const;
/*(5)*/ async::task<expected<vector<byte>, error>> async_read(const entry& e) const noexcept;
/*(6)*/ async::task<expected<vector<byte>, error>> async_read(entry e, limits l) const noexcept;
```

Reads an entry's data whole, decompressed and checked as its [reader](reader.md) checks it. The entry's size, as
the central directory gives it, is held against the limits' `max_size` before a byte is read, so that neither a zip
bomb nor entries overlapping the same data take more than the limit; the vector is made at that size.

- (1), (3), (5) With the default [limits](../limits.md): 1 GiB.
- (2), (4), (6) With the limits given.
- (3–4) The first entry of the name.
- (5–6) Return a task that reads in the same way; a read of a file runs on the
  [blocking pool](../../async/spawn_blocking.md), into the vector's managed memory, and the task lets the worker go
  every 64 KB decompressed.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the entry |
| `name` | the entry's name |
| `l` | the bound on the entry's size ([limits](../limits.md)) |

## Return value

The data, or the [error](../error.md): the entry past `max_size` (`errc::too_large`), no entry of the name
(`errc::invalid_argument`), the entry's own errors (`errc::corrupt`, `errc::checksum`, `errc::unsupported`), a failure
of the source (`errc::io`).

## Complexity

Linear in the size of the entry.

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
    compress::zip::writer w(archive);
    (void)w.add("big.txt", string("x").repeat(1 << 20));
    (void)w.close();

    auto a = compress::zip::archive::from(archive.data());
    println("{}", a->read("big.txt")->size());
    println("{}", a->read("big.txt", {.max_size = 1000}).error().message());
}
```

Output:

```text
1048576
offset 0: zip: entry big.txt: larger than the limit
```

## See also

- [reader](reader.md): the data as a stream
- [limits](../limits.md)
- [sgcl::compress::zip::archive](../zip-archive.md)
