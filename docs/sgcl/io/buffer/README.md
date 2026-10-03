[sgcl](../../README.md) › [io](../README.md)

# sgcl::io::buffer

```cpp
#include "sgcl/io/stream.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class buffer;   // : public mixin::reader<buffer>, public mixin::writer<buffer>,
                    //   public mixin::seeker<buffer>
}
```

`sgcl::io::buffer` is a growing block of bytes in memory, read from the front and written at the back: a parser's
input, a response being built, the stream of a test. It is Go's `bytes.Buffer`, a reader and a writer over a
`vector<byte>` inside a managed object. A read consumes: [data](data.md) is what remains, a view valid until
the next write, and [text](text.md) the same as a string. Its async forms never wait. As the source of
`io::copy` it hands over what it holds in one write ([write_to](write_to.md)).

It is a handle of one word, as a [file](../file/README.md) is: a `tracked_ptr` to the state, so a copy is the same buffer, and
both copies write to and read from the same bytes; a stream made of one ([reader](../reader/README.md), [writer](../writer/README.md))
holds the state, not the handle. `io::buffer out;` makes its empty state at once, as every constructor does, so
that a copy always shares it.

It seeks as a file does, for writing: the write position counts from the first byte held, follows the end until a
seek moves it, and a write there overwrites what is held and runs on past the end; a seek past the end is allowed,
and a write there fills the gap with zeros, as Go's `os.File` and `pwrite` do, where a `std::stringstream` refuses
the seek. Reads still consume from the front, and the position moves back with the bytes they take. It is what a
writer that must come back to its start needs in memory: a [7z archive](../../compress/sevenzip.md) writes its
signature header last. `std::stringstream` is the nearest standard counterpart, with a position for reading and one
for writing and a copy that is a new stream.

## Rules

- A buffer holds a `tracked_ptr`, so it lives where one may: on a stack, in a task, in a managed object
  ([The rules](../../core/README.md#the-rules), 1). In a global or a `std` container, a [rooted](../../core/rooted/README.md) of
  it: `rooted<io::buffer> log(std::in_place);`, then `log->write(...)`; never a root in a managed object or a task's
  frame, since a root is never part of a cycle.
- It is a [handle](../../core/req/handle.md): a buffer variable that one thread replaces while others read it is an
  [atomic](../../core/atomic-handle/README.md) of it, one word.
- One thread or task at a time on one buffer, its copies included.
- [data](data.md) is valid until the next write; [release](release.md) takes the bytes out as a
  vector of their own.
- The buffer's own [size](size.md) (the bytes held) hides the one of [mixin::seeker](../mixin/seeker/README.md).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](buffer.md) | constructs the buffer, empty or holding bytes |
| `(destructor)` | drops the word; the state is left to the collector |
| [operator=](operator_assign.md) | makes this handle the same buffer as another |

#### Operations

| Function | Description |
|---|---|
| [read, async_read](read.md) | takes bytes from the front |
| [write, async_write](write.md) | writes bytes at the write position |
| [seek](seek.md) | moves the write position |
| [write_to, async_write_to](write_to.md) | writes everything held to a writer, in one write |
| [clear](clear.md) | drops every byte held |
| [reserve](reserve.md) | makes room for bytes to come |
| [release](release.md) | takes the bytes out, leaving the buffer empty |

#### Observers

| Function | Description |
|---|---|
| [data](data.md) | the bytes held, as a slice |
| [text](text.md) | the bytes held, as a string |
| [size](size.md) | the number of bytes held |
| [empty](empty.md) | checks whether the buffer holds no bytes |

#### From mixin::reader

The rest of what a reader does, each an algorithm of io over this stream ([mixin::reader](../mixin/reader/README.md)).

| Function | Description |
|---|---|
| [read_full, async_read_full](../mixin/reader/read_full.md) | fills the whole buffer given |
| [read_all, async_read_all](../mixin/reader/read_all.md) | everything held, taken as bytes |
| [read_all_text, async_read_all_text](../mixin/reader/read_all_text.md) | everything held, taken as a string |
| [copy_to, async_copy_to](../mixin/reader/copy_to.md) | everything held, written to a writer |

#### From mixin::writer

The rest of what a writer does ([mixin::writer](../mixin/writer/README.md)).

| Function | Description |
|---|---|
| [write, async_write](../mixin/writer/write.md) | writes a text or one byte |
| [copy_from, async_copy_from](../mixin/writer/copy_from.md) | everything from a reader to its end, written here |

#### From mixin::seeker

The position, over [seek](seek.md) ([mixin::seeker](../mixin/seeker/README.md)); its `size` is hidden by the buffer's
own.

| Function | Description |
|---|---|
| [tell](../mixin/seeker/tell.md) | the write position |
| [rewind](../mixin/seeker/rewind.md) | moves the write position to the first byte held |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | checks whether two handles are the same buffer |

## Complexity

- A copy of the handle: one word.
- A read: linear in the bytes read. A write at the end: amortized linear in the bytes written, the vector growing by
  doubling; a write after a seek: linear in the bytes written, plus the zeros of a gap.
- [write_to](write_to.md): one write of everything held.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    io::buffer message;
    message.write("LEN:????\n");
    message.write("the payload\n");

    auto length = message.size() - 9;  // the payload after the header
    message.seek(4);
    message.write(txt::format("{:04}", length));

    auto header = io::limit_reader(message, 9).read_all_text();
    print("{}", *header);
    println("{} bytes of payload left", message.size());
    io::copy(io::stdout, message);
}
```

Output:

```text
LEN:0012
12 bytes of payload left
the payload
```

## See also

- [buffered_reader](../buffered_reader/README.md), [buffered_writer](../buffered_writer/README.md): a block in front of a stream
- [file](../file/README.md): the stream over a descriptor
- [reader](../reader/README.md), [writer](../writer/README.md): any stream, as a value
- [rooted](../../core/rooted/README.md), [atomic](../../core/atomic-handle/README.md): a handle outside the managed heap, a handle shared
  between threads
