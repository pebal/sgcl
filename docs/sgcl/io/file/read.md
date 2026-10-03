[sgcl](../../README.md) › [io](../README.md) › [file](README.md)

# sgcl::io::file::read, async_read

```cpp
expected<size_t, error> read(const slice<byte>& buffer) const;                                // (1)
async::task<expected<size_t, error>> async_read(const slice<byte>& buffer) const noexcept;    // (2)
```

Reads what is available at the position into `buffer`, at most its size, and moves the position past it: the
`read(2)` of the descriptor, made again when a signal interrupts it.

1. On the calling thread. On a blocking descriptor (a regular file, a terminal) the call waits in the kernel; on a
   non-blocking one (a pipe from [pipe](../pipe.md), a FIFO) a read that would block waits on the
   [reactor](../../async/readable.md) for readiness, the thread held meanwhile.
2. The same for a task, which holds no worker while it waits: a regular file's (and a terminal's) read runs on the
   [blocking pool](../../async/spawn_blocking.md), a disk having no readiness to wait for; a non-blocking descriptor's
   waits for readiness on the reactor and is made by the task when it will not block.

## Parameters

| Parameter | Description |
|---|---|
| `buffer` | where the bytes go; its size is the most read |

## Return value

The number of bytes read, fewer than the size of `buffer` when fewer were there; 0 at the end of the file, and for
an empty `buffer`. Or the [error](../error/README.md), its operation `read` and its path the file's:

- `errc::closed` when the file was closed before the call, or while it waited ([close](close.md));
- `ECANCELED` when the reactor stopped while it waited;
- the `errno` of `read(2)` otherwise (`EBADF` for a file opened only for writing).

## Complexity

One system call, linear in the bytes read; on a non-blocking descriptor, one more per wait for readiness.

## Exceptions

- (1) `std::system_error` when the descriptor is non-blocking, the read has to wait, and the thread of the reactor,
  or of the timers, which its first use starts, cannot be made.
- (2) None: the task's own exceptions are the task's.

## Notes

The end is not an error: a read of 0 is how a file says it has no more, as in Go. To fill the whole buffer or fail,
[read_full](../mixin/reader/read_full.md); to read to the end, [read_all](../mixin/reader/read_all.md).

`buffer` handed to (2) without an owner (a plain array, a `std::span`) on a regular file is read into a managed
block on the pool and copied into `buffer` when the task resumes; a slice with an owner (a `vector`, an
[array](../../core/array/README.md), a [buffer](../buffer/README.md)'s) is read into as it is.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::write_file("digits.txt", "0123456789");
    io::file f = io::open("digits.txt");
    byte chunk[4];
    for (;;) {
        size_t n = f.read(chunk).value();
        println("read {}", n);
        if (n == 0) {
            break;
        }
    }
}
```

Output:

```text
read 4
read 4
read 2
read 0
```

A task reads a regular file on the pool and a pipe on the reactor:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> show(io::file f) {
    vector<byte> buffer(64);
    auto n = co_await f.async_read(buffer);
    println("{}: {} bytes, nonblocking {}", f.path(), *n, f.is_nonblocking());
}

int main() {
    io::write_file("digits.txt", "0123456789");
    auto [in, out] = io::pipe().value();
    out.write("hello");
    async::run(show(io::open("digits.txt")));
    async::run(show(in));
}
```

Output:

```text
digits.txt: 10 bytes, nonblocking false
pipe: 5 bytes, nonblocking true
```

## See also

- [write, async_write](write.md): the other direction
- [read_at, async_read_at](read_at.md): a read at an offset, the position untouched
- [read_full](../mixin/reader/read_full.md), [read_all](../mixin/reader/read_all.md): a whole buffer, the whole
  file
- [buffered_reader](../buffered_reader/README.md): lines over a file
- [sgcl::io::file](README.md)
