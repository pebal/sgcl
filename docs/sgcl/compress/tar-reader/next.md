[sgcl](../../README.md) › [compress](../README.md) › [tar](../tar.md) › [reader](README.md)

# sgcl::compress::tar::reader::next, async_next

```cpp
expected<optional<entry>, error> next();                                // (1)
async::task<expected<optional<entry>, error>> async_next() noexcept;    // (2)
```

Goes to the next entry: skips what is left of the current entry's data, reads the headers before the next one —
global and per-entry pax records, GNU's long names, the ustar header — and gives the [entry](../tar-entry/README.md) they
make together. The reads give its data from then on. At the end of the archive it gives `nullopt`, and so on every
call after.

A sparse file or a multi-volume part is `errc::unsupported` naming the entry, and the next call goes on to the entry
after it. Every other failure stops the reader: this call and every call after give the same error.

1. Blocks the calling thread for the reads of `in`.
2. Returns a task that does the same and gives the worker back while `in` reads.

## Parameters

None.

## Return value

The next entry, `nullopt` at the end of the archive, or the [error](../error/README.md): a header that cannot be read or a
block of zeros followed by more (`errc::invalid_header`), a header checksum that does not match (`errc::checksum`), a
pax header or a long name past 1 MiB (`errc::too_large`), a sparse file or a multi-volume part
(`errc::unsupported`), data cut short (`errc::unexpected_end`), a failure of `in` (`errc::io`).

## Complexity

Linear in the size of the headers and of the data skipped.

## Exceptions

- (1) What the `read` of `in` throws.
- (2) None: the task carries what it throws.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer archive;
    compress::tar::writer w(archive);
    for (string name : {"one.txt", "two.txt", "three.txt"}) {
        (void)w.write_header({.name = name, .size = name.size()});
        (void)w.write(name);
    }
    (void)w.close();

    compress::tar::reader r(archive);
    while (auto e = r.next()) {  // the data not read is skipped
        if (!*e) {
            break;
        }
        println("{} ({} bytes)", (*e)->name, (*e)->size);
    }
}
```

Output:

```text
one.txt (7 bytes)
two.txt (7 bytes)
three.txt (9 bytes)
```

## See also

- [read](read.md): the entry's data
- [sgcl::compress::tar::reader](README.md)
