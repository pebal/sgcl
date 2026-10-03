[sgcl](../../README.md) › [io](../README.md) › [buffered_writer](../buffered_writer.md)

# sgcl::io::buffered_writer::write, async_write

```cpp
/*(1)*/ expected<size_t, error> write(const slice<const byte>& data) const;
/*(2)*/ async::task<expected<size_t, error>> async_write(const slice<const byte>& data) const
            noexcept;
```

Writes `data` into the block. When the block is full it is written to the stream underneath and filled again; while
the block is empty, a part of at least the block's size (8 KB) goes to the stream directly, with no copy. What is
left in the block waits for the next write, a [flush](flush.md) or the [close](close.md). The text and the byte forms,
`write("text")` and `write(byte('\n'))`, are [mixin::writer's](../mixin/writer/write.md), brought back beside these.

After an error the writer has kept (a failure of the stream, a write after the close), a write gives that error at
once and writes nothing; a write after the close is `errc::closed`, kept as the first error when there is none.

1. Writes on the calling thread.
2. The same for a task: the block is moved into managed memory by the first async operation, once for the writer's
   life, and written with the stream's `async_write`, or its `write` on the blocking pool for a stream that has only
   that. `data` is written from where it lies: a slice with an owner (a `string`'s, a `vector`'s) keeps it in the
   task's frame. The writer is held by the handle the task was made from: the caller keeps it alive until the task is
   done.

## Parameters

| Parameter | Description |
|---|---|
| `data` | the bytes to write |

## Return value

`data.size()`, all of it taken, or the error: the one the writer kept, or the error of the stream's write, which
it keeps.

## Complexity

Linear in `data.size()`: a copy into the block, a write of the stream per block filled.

## Exceptions

- (1) What the write of the stream underneath throws: a [file](../file/write.md)'s `std::system_error` when the
  reactor's thread cannot be started.
- (2) None. What the write throws is the task's: its `co_await` rethrows it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer out;
    io::buffered_writer w(out);
    vector<byte> small(100);
    w.write(small);
    println("{} buffered, {} written", w.buffered(), out.size());

    vector<byte> large(20000);
    w.async_write(large).wait();  // the block filled and written, the rest directly
    println("{} buffered, {} written", w.buffered(), out.size());
}
```

Output:

```text
100 buffered, 0 written
0 buffered, 20100 written
```

## See also

- [mixin::writer::write](../mixin/writer/write.md): text and one byte
- [flush](flush.md): writes what the block holds
- [sgcl::io::buffered_writer](../buffered_writer.md)
