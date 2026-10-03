[sgcl](../../README.md) › [compress](../README.md) › [tar](../tar.md) › [writer](../tar-writer.md)

# sgcl::compress::tar::writer::write_header, async_write_header

```cpp
expected<void, error> write_header(const entry& e);                         // (1)
async::task<expected<void, error>> async_write_header(entry e) noexcept;    // (2)
```

Starts an entry: pads the data of the one before to its block, and writes the header of `e` — ustar when it fits,
a pax extended header before it when it does not (the fields that need one are on the
[writer](../tar-writer.md)'s page). The entry's data, exactly `e.size` bytes, follows through [write](write.md);
a directory, a link, a device or a fifo has none, and its size must be 0.

1. Blocks the calling thread for the write of `out`.
2. Returns a task that does the same and gives the worker back while `out` writes. `e` is copied into the task.

## Parameters

| Parameter | Description |
|---|---|
| `e` | the entry: its name, type, size, mode, times, owners, records |

## Return value

Nothing, or the [error](../error.md), kept as the writer's first: the data of the entry before not whole, an entry
the format cannot hold (an empty name, a NUL in a name, a size for a type that has no data, a device number past
2 097 151, a pax record that is malformed, given twice or one of the fields'), all `errc::invalid_argument`; a call
after the close; a failure of `out` (`errc::io`); or the error kept from before.

## Complexity

Linear in the size of the header.

## Exceptions

- (1) What the `write` of `out` throws.
- (2) None: the task carries what it throws.

## Example

```cpp
#include "sgcl/compress.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer archive;
    compress::tar::writer w(archive);
    compress::tar::entry e{.name = string("a/").repeat(60) + "deep.txt", .size = 2};
    (void)w.write_header(e);  // a name past ustar's: a pax header before it
    (void)w.write("hi");
    println("{}", w.write_header({.name = "", .size = 0}).error().message());
}
```

Output:

```text
tar: an entry with no name
```

## See also

- [write](write.md): the entry's data
- [add_file](add_file.md): a file as a whole entry
- [tar::entry](../tar-entry.md)
- [sgcl::compress::tar::writer](../tar-writer.md)
