[sgcl](../../README.md) › [compress](../README.md) › [tar](../tar.md) › [reader](README.md)

# sgcl::compress::tar::reader::read, async_read

```cpp
expected<size_t, io::error> read(const slice<byte>& out);                         // (1)
async::task<expected<size_t, io::error>> async_read(slice<byte> out) noexcept;    // (2)
```

Reads the current entry's data into `out`: exactly the entry's size in all, then 0. Before the first entry and
after the last it gives 0. Data that ends inside the entry is the error of the read that reaches it.

1. Blocks the calling thread for the reads of `in`.
2. Returns a task that does the same and gives the worker back while `in` reads. The bytes go into `out` when the
   task runs: the buffer lives until the task is done.

## Parameters

| Parameter | Description |
|---|---|
| `out` | the buffer the data goes into |

## Return value

The number of bytes put into `out`, 0 at the end of the entry, or an error: an `io::error` of the
[compress category](../compress_category.md) naming the entry (the whole [error](../error/README.md) is in
[last_error](last_error.md)), or the error of `in` as it came.

## Complexity

Linear in the bytes read.

## Exceptions

- (1) What the `read` of `in` throws; the errors of the stream and of the archive are returned.
- (2) None: the task carries what it throws.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer archive;
    compress::tar::writer w(archive);
    (void)w.write_header({.name = "notes.txt", .size = 19});
    (void)w.write("one\ntwo\nthree\nfour\n");
    (void)w.close();

    compress::tar::reader r(archive);
    (void)r.next();
    io::buffered_reader lines(r);  // the lines of the entry
    for (auto line : lines.lines()) {
        println("[{}]", line);
    }
}
```

Output:

```text
[one]
[two]
[three]
[four]
```

## See also

- [next](next.md)
- [sgcl::compress::tar::reader](README.md)
