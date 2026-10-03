[sgcl](../../../README.md) › [io](../../README.md) › [mixin](../README.md) › [reader](../reader.md)

# sgcl::io::mixin::reader\<Derived\>::read_all, async_read_all

```cpp
/*(1)*/ expected<vector<byte>, error> read_all()
            noexcept(noexcept(io::read_all(std::declval<Derived&>())));
/*(2)*/ async::task<expected<vector<byte>, error>> async_read_all() noexcept
            requires req::async_reader<Derived&>;
```

Reads this stream to its end and returns its bytes. It is [io::read_all](../../read_all.md) over this stream:
gathered in unmanaged memory and copied once into a vector of exactly their size (1), or read into managed blocks
that a read on the blocking pool holds and copied once at the end (2).

1. Reads on the calling thread.
2. The same for a task, with `Derived`'s `async_read`; takes part only when `Derived` has it. The stream is held by
   reference while the task runs: the caller keeps it alive until the task is done.

## Parameters

None.

## Return value

The bytes from the position to the end, an empty vector for a stream at its end, or the error of a read, as the
stream gave it; the bytes read before it are dropped.

## Complexity

Linear in the number of bytes read.

## Exceptions

- (1) What `Derived`'s `read` throws; none when it is noexcept.
- (2) None. What a read throws is the task's: its `co_await` rethrows it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::reader any = io::buffer("bytes through a handle");
    auto bytes = any.read_all();
    println("{} bytes", bytes->size());

    io::buffer two("ab");
    auto later = two.async_read_all().wait();
    println("{} {}", later->size(), char(later->front()));
}
```

Output:

```text
22 bytes
2 a
```

## See also

- [io::read_all](../../read_all.md): the same over any stream, and how the bytes are gathered
- [read_all_text](read_all_text.md): the same as a string
- [sgcl::io::mixin::reader\<Derived\>](../reader.md)
