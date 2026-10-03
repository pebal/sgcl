[sgcl](../../../README.md) › [io](../../README.md) › [mixin](../README.md) › [writer](../writer.md)

# sgcl::io::mixin::writer\<Derived\>::copy_from, async_copy_from

```cpp
template<req::reader R>
expected<size_t, error> copy_from(R&& r)                                                 // (1)
    noexcept(noexcept(io::copy(std::declval<Derived&>(), std::forward<R>(r))));
template<req::async_reader R>
async::task<expected<size_t, error>> async_copy_from(R&& r) noexcept(/* see below */)    // (2)
    requires req::async_writer<Derived&>;
```

Writes everything the stream `r` gives, from its position to its end, to this stream. It is
[io::copy(w, r)](../../copy.md) over this stream, through one block of 32 KB or in one call where one of the two
has a way of its own.

1. Copies on the calling thread.
2. The same for a task, with `r`'s `async_read` and `Derived`'s `async_write`; takes part only when `Derived` has
   `async_write`. This stream is held by reference while the task runs, and so is `r` when it is given by reference:
   the caller keeps them alive until the task is done. An `r` given as a temporary is moved into the task's frame.

## Parameters

| Parameter | Description |
|---|---|
| `r` | the stream to read, to its end: any [reader](../../req/reader.md) (2: async reader) |

## Return value

The number of bytes copied, or the first error of a read or a write, as the stream gave it; what was written before
it stays written.

## Complexity

Linear in the number of bytes copied.

## Exceptions

- (1) What `r`'s `read` and `Derived`'s `write` throw; none when they are noexcept.
- (2) What the move of an `r` given as a temporary into the task's frame throws; none for one given by reference.
  What a read or a write throws is the task's: its `co_await` rethrows it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer out;
    out.write("head, ");
    out.copy_from(io::buffer("body, "));
    auto n = out.async_copy_from(io::buffer("tail")).wait();
    println("{} {}", *n, out.text());
}
```

Output:

```text
4 head, body, tail
```

## See also

- [io::copy](../../copy.md): the same over any two streams, and the ways of their own
- [mixin::reader::copy_to](../reader/copy_to.md): the same from the reader's side
- [sgcl::io::mixin::writer\<Derived\>](../writer.md)
