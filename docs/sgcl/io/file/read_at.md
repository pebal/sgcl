[sgcl](../../README.md) › [io](../README.md) › [file](README.md)

# sgcl::io::file::read_at, async_read_at

```cpp
expected<size_t, error> read_at(const slice<byte>& buffer, uint64_t offset) const noexcept;    // (1)
async::task<expected<size_t, error>> async_read_at(const slice<byte>& buffer,                  // (2)
                                                   uint64_t offset) const noexcept;
```

Reads into `buffer`, at most its size, from `offset` bytes from the beginning of the file: the `pread(2)` of the
descriptor, made again when a signal interrupts it. The file's own position is untouched, so that any number of
threads and tasks read one file at once without a seek between them.

1. On the calling thread.
2. The same for a task, on the [blocking pool](../../async/spawn_blocking.md), whatever the descriptor.

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | where the bytes go; its size is the most read |
| `offset` | where the read starts, from the beginning of the file |

## Return value

The number of bytes read, fewer than the size of `buffer` when the end comes first; 0 at or past the end. Or the
[error](../error/README.md), its operation `read_at` and its path the file's: `errc::closed` for a closed file, otherwise
the `errno` of `pread(2)` (`ESPIPE` for a pipe, which has no offsets).

## Complexity

One system call, linear in the bytes read.

## Exceptions

None.

## Notes

`buffer` handed to (2) without an owner (a plain array, a `std::span`) is read into a managed block on the pool and
copied into `buffer` when the task resumes; a slice with an owner (a `vector`, an [array](../../core/array/README.md)) is
read into as it is.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::write_file("records.txt", "aaaabbbbcccc");
    io::file f = io::open("records.txt");
    vector<byte> record(4);
    f.read_full(record);  // the first record, by the position
    println("{}", *f.tell());

    size_t n = f.read_at(record, 8).value();
    println("{} {}", n, string(record));
    println("{} {}", *f.tell(), *f.read_at(record, 12));
}
```

Output:

```text
4
4 cccc
4 0
```

Tasks read the records of one file at once:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<string> record(io::file f, uint64_t offset) {
    vector<byte> buffer(4);
    size_t n = (co_await f.async_read_at(buffer, offset)).value();
    co_return string(buffer.as_slice(0, n));
}

int main() {
    io::write_file("records.txt", "aaaabbbbcc");
    io::file f = io::open("records.txt");
    auto first = async::spawn(record(f, 0));
    auto second = async::spawn(record(f, 4));
    auto third = async::spawn(record(f, 8));
    println("{} {} {}", first.wait(), second.wait(), third.wait());
}
```

Output:

```text
aaaa bbbb cc
```

## See also

- [write_at, async_write_at](write_at.md): the other direction
- [read, async_read](read.md): a read at the position
- [sgcl::io::file](README.md)
