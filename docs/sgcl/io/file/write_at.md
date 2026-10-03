[sgcl](../../README.md) › [io](../README.md) › [file](../file.md)

# sgcl::io::file::write_at, async_write_at

```cpp
expected<size_t, error> write_at(const slice<const byte>& data,                         // (1)
                                uint64_t offset) const noexcept;
async::task<expected<size_t, error>> async_write_at(const slice<const byte>& data,      // (2)
                                                    uint64_t offset) const noexcept;
```

Writes the whole of `data` at `offset` bytes from the beginning of the file: the `pwrite(2)` of the descriptor, made
again for the rest after a short write and when a signal interrupts it. The file's own position is untouched, so
that any number of threads and tasks write one file at once without a seek between them. A write past the end
leaves a hole of zeros before it.

1. On the calling thread.
2. The same for a task, on the [blocking pool](../../async/spawn_blocking.md), whatever the descriptor.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to write |
| `offset` | where the write starts, from the beginning of the file |

## Return value

The size of `data`: everything was written. Or the [error](../error.md), its operation `write_at` and its path the
file's: `errc::closed` for a closed file, otherwise the `errno` of `pwrite(2)` (`EBADF` for a file opened only for
reading, `ESPIPE` for a pipe).

## Complexity

Linear in the size of `data`: one system call, more after a short write.

## Exceptions

None.

## Notes

The data of (2) stays alive while the task awaits. Given without an owner (a plain array, a `std::span`), it is
copied into a managed block before the write starts, since the pool's thread may outlive the frame of a task let go
of; a slice with an owner (a `string`, a `vector`) is written as it is.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::write_file("data.bin", "HEADER..body..........FOOTER..");
    io::file data = io::open("data.bin", io::open_flags::read | io::open_flags::write);
    vector<byte> header(6);
    data.read_full(header);
    uint64_t size = data.size().value();  // the end, the position kept
    data.write_at(string("footer"), size - 8);
    println("{}", *data.tell());  // still after the header
    data.sync();
    data.close();
    println("{}", *io::read_text("data.bin"));
}
```

Output:

```text
6
HEADER..body..........footer..
```

## See also

- [read_at, async_read_at](read_at.md): the other direction
- [write, async_write](write.md): a write at the position
- [sgcl::io::file](../file.md)
