[sgcl](../../../README.md) › [io](../../README.md) › [mixin](../README.md) › [reader](../reader.md)

# sgcl::io::mixin::reader\<Derived\>::copy_to, async_copy_to

```cpp
/*(1)*/ template<req::writer W>
        expected<size_t, error> copy_to(W&& w)
            noexcept(noexcept(io::copy(std::forward<W>(w), std::declval<Derived&>())));
/*(2)*/ template<req::async_writer W>
        async::task<expected<size_t, error>> async_copy_to(W&& w) noexcept(/* see below */)
            requires req::async_reader<Derived&>;
```

Copies this stream from its position to its end into the stream `w`. It is [io::copy(w, r)](../../copy.md) over
this stream, through one block of 32 KB or in one call where one of the two has a way of its own.

1. Copies on the calling thread.
2. The same for a task, with `Derived`'s `async_read` and `w`'s `async_write`; takes part only when `Derived` has
   `async_read`. This stream is held by reference while the task runs, and so is `w` when it is given by
   reference: the caller keeps them alive until the task is done. A `w` given as a temporary is moved into the
   task's frame.

## Parameters

| Parameter | Description |
|---|---|
| `w` | the stream to write to: any [writer](../../req/writer.md) (2: async writer) |

## Return value

The number of bytes copied, or the first error of a read or a write, as the stream gave it; what was written before
it stays written.

## Complexity

Linear in the number of bytes copied.

## Exceptions

- (1) What `Derived`'s `read` and `w`'s `write` throw; none when they are noexcept.
- (2) What the move of a `w` given as a temporary into the task's frame throws; none for one given by reference.
  What a read or a write throws is the task's: its `co_await` rethrows it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer source("to the standard output\n");
    auto n = source.copy_to(io::stdout);
    println("{} bytes", *n);

    io::buffer out;
    io::buffer("into a buffer").async_copy_to(out).wait();
    println("{}", out.text());
}
```

Output:

```text
to the standard output
23 bytes
into a buffer
```

## See also

- [io::copy](../../copy.md): the same over any two streams, and the ways of their own
- [mixin::writer::copy_from](../writer/copy_from.md): the same from the writer's side
- [sgcl::io::mixin::reader\<Derived\>](../reader.md)
