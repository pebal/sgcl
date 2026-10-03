[sgcl](../../../README.md) › [hash](../../README.md) › mixin › [hasher](README.md)

# sgcl::hash::mixin::hasher\<Derived\>::copy_from, async_copy_from

```cpp
expected<size_t, io::error> copy_from(const io::reader& r);                                // (1)
async::task<expected<size_t, io::error>> async_copy_from(const io::reader& r) noexcept;    // (2)
```

Reads `r` to its end and hashes every byte it gives after what the hasher took before: Go's `io.Copy(h, r)`. `r` is
any stream an [io::reader](../../../io/reader/README.md) holds: a file, a connection, a pipe, an `io::buffer`, a lambda
that fills a slice.

1. On the calling thread, through one block of io's copy size on its stack, as `io::copy` reads; each read goes to
   the class's `update` straight from the block.
2. The same in a task, for `co_await h.async_copy_from(r)`. Its block is managed, since its reads may run on the
   blocking pool and the slice a read is given holds the block. The task keeps a copy of the reader and refers to
   the hasher, which the caller keeps alive until the task is done, as `co_await` in one statement does.

- (1–2) When `r` fails, what was read before the failure has been hashed, and the error is returned as it came.

## Parameters

| Parameter | Description |
|---|---|
| `r` | the stream to read to its end |

## Return value

The number of bytes read and hashed, or the error of `r`; (2) as the result of the task.

## Complexity

Linear in the number of bytes the stream gives.

## Exceptions

- (1) What a read of `r` throws: the read function of a stream the program made itself, or `std::system_error` when
  a read that waits on io's reactor (a pipe, a socket) has to start the reactor's thread and cannot.
- (2) None from the call; what a read throws comes out of the `co_await` of the task.

What was read before the exception has been hashed.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("notes.txt", "first line\nsecond line\n");

    // a file already open, read from its position to its end
    auto file = io::open("notes.txt");
    hash::crc32 h;
    auto n = h.copy_from(*file);
    println("{} {:08x}", *n, h.value());
    println("{}", h.value() == hash::crc32::of("first line\nsecond line\n"));

    // a keyed hasher, in a task
    array<byte, 16> key = {};
    hash::siphash s(key);
    auto m = async::run(s.async_copy_from(io::buffer("abc")));
    println("{} {}", *m, s.value() == hash::siphash::of("abc", key));
}
```

Output:

```text
23 5d455a2c
true
3 true
```

## See also

- [of_file](of_file.md): the hash of a whole file, through `copy_from`
- [update](update.md): bytes already in memory
- [io::copy](../../../io/copy.md): the same reading into a writer
- [sgcl::hash::mixin::hasher\<Derived\>](README.md)
