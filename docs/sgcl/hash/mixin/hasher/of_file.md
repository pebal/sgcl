[sgcl](../../../README.md) › [hash](../../README.md) › mixin › [hasher](README.md)

# sgcl::hash::mixin::hasher\<Derived\>::of_file, async_of_file

```cpp
static auto of_file(const string& path) requires std::default_initializable<Derived>;                   // (1)
static auto async_of_file(const string& path) noexcept requires std::default_initializable<Derived>;    // (2)
```

The hash of the whole file at `path`: the one-shot form of a file, what [of](of.md) of its bytes gives,
`of(io::read_file(path))`, without the file held in memory. The file is opened, read to its end by
[copy_from](copy_from.md) and closed.

1. On the calling thread; the block of the reads on its stack.
2. The same in a task, for `co_await T::async_of_file(path)`; the block managed, since its reads may run on the
   blocking pool. The task keeps its own copy of `path`, so the caller's string may go before the task runs.

- (1–2) Only for a class made without arguments, as `of(data)` is: a hasher with a key
  ([siphash](../../siphash/README.md)) has neither, and opens the file and calls `copy_from` itself. A class with a seed
  hashes as one made without it: [xxh3_64](../../xxh3_64/README.md), [xxh3_128](../../xxh3_128/README.md),
  [xxh32](../../xxh32/README.md) and [xxh64](../../xxh64/README.md) with the seed 0,
  [maphash](../../maphash/README.md) with the process's seed.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the path of the file |

## Return value

- (1) `expected<V, io::error>`, where `V` is the type of the class's `value()`: the hash of the file's bytes, or
  the error of the open or of a read; a path that is not there is an error with `is_not_found()`.
- (2) `async::task` of the same.

## Complexity

Linear in the size of the file.

## Exceptions

- (1) None for a regular file. A FIFO or a device is read through io's reactor, and a read there throws
  `std::system_error` when it has to start the reactor's thread and cannot.
- (2) None from the call; what a read throws comes out of the `co_await` of the task.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    (void)io::write_file("archive.bin", "first line\nsecond line\n");

    auto crc = hash::crc32::of_file("archive.bin");
    println("{:08x}", *crc);
    println("{}", *crc == hash::crc32::of("first line\nsecond line\n"));

    // in a task
    auto sum = async::run(hash::xxh3_64::async_of_file("archive.bin"));
    println("{:016x}", *sum);

    auto missing = hash::crc32::of_file("missing.bin");
    println("{} {}", missing.has_value(), missing.error().is_not_found());
}
```

Output:

```text
5d455a2c
true
b234318c55590dc2
false true
```

## See also

- [copy_from](copy_from.md): a stream already open, or a file with a keyed hasher
- [of](of.md): bytes already in memory
- [io::read_file](../../../io/file/README.md): the whole file in memory
- [sgcl::hash::mixin::hasher\<Derived\>](README.md)
