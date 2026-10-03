[sgcl](../../README.md) › [io](../README.md)

# sgcl::io::reader

```cpp
#include "sgcl/io/stream.h"   // or "sgcl/io.h"

namespace sgcl::io {
    class reader;   // : public mixin::reader<reader>
}
```

`sgcl::io::reader` is any reader, held as a value: whatever meets [req::reader](../req/reader.md) or
[req::async_reader](../req/reader.md) — a handle of the library (a [file](../file/README.md), a [buffer](../buffer/README.md), a
[buffered_reader](../buffered_reader/README.md), a `net::connection`), a stream of the program's, a lambda — kept where a
stream must be kept: the source of a `buffered_reader`, the input of a decoder, a field of a class. It is three
words: a `tracked_ptr` of the object that keeps the stream, a pointer to the stream and a pointer to a table of its
methods, made once per type. That is Go's `io.Reader` interface value, the method table beside the pointer, with
`close` as Go's `io.ReadCloser` has it: a file held as an `io::reader` is closed through it. The standard library
has no counterpart: a `std::istream` is a class of a hierarchy over a virtual `std::streambuf`, where the stream
here declares nothing virtual and derives from nothing ([req](../req/README.md)).

What the reader keeps depends on how the stream is given. A handle of the library is held by its object, the one
the copies of the handle share, so the handle it was made of may go first. A stream given by reference is
referenced, and the managed object it lies in is kept, if it lies in one; one on a stack, a global (`io::stdin`) or
one a `unique_ptr` owns is the caller's to keep alive. A stream given by `tracked_ptr` is held by it, a `unique_ptr`
from `make_tracked` given as a temporary is taken over, and a callable or a temporary is copied into a managed
object of its own. A raw pointer is not taken.

The half a stream lacks is made from the other, and only here: [read](read.md) of a stream that has only
`async_read` waits for its task on the calling thread, `async_read` of a stream that has only `read` runs it on the
[blocking pool](../../async/spawn_blocking.md), a thread of which it holds for the whole wait, as a socket read waiting for
data would. [has_read](has_read.md) and [has_async_read](has_async_read.md) tell which halves are the
stream's own; `io::reader(r)` written out is where the choice is made. The functions of io take a stream as it
is and never make a half: `io::async_copy` of a source that has only `read` does not compile.

## Rules

- A reader holds a `tracked_ptr`, so it lives where one may: on a stack, in a task, in a managed object; in a
  global or a `std` container, a [rooted](../../core/rooted/README.md) of it ([The rules](../../core/README.md#the-rules), 1).
- A copy is the same stream: the three words copied, the stream shared.
- One thread or task at a time on one stream, as with the stream itself.
- An async read that runs on the blocking pool (the async half made of a stream's `read`) and is given a slice
  without an owner goes through a managed block: the bytes read are copied back from it when the task resumes. The
  pool's thread may outlive the frame of a task let go of, and it never touches plain memory that died with it. A
  slice with an owner (a `string`, a `vector`, a `buffer`, any type of the library) is used as it is, with no copy.
- A stream's destructor runs on the collector's thread, after the sweep that finds the object dead, which may be
  long after the last use: a stream that holds a descriptor is closed by [close](close.md) when done, which
  releases it now and reports the error a deferred close cannot.
- Errors are values, `expected<T, error>` ([error](../error/README.md)); the end of the stream is not an error, a read
  returns 0.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](reader.md) | constructs the reader, empty or holding a stream |
| `(destructor)` | drops the words; the stream is not closed |
| `operator=` | copies or moves the three words of another reader |

#### Operations

| Function | Description |
|---|---|
| [read, async_read](read.md) | reads bytes from the stream |
| [close, async_close](close.md) | closes the stream, when it has a close |

#### Observers

| Function | Description |
|---|---|
| [has_read](has_read.md) | checks whether `read` is the stream's own |
| [has_async_read](has_async_read.md) | checks whether `async_read` is the stream's own |
| [has_close](has_close.md) | checks whether the stream has a close |
| [fd](fd.md) | the descriptor under the stream, -1 for none |
| [operator bool](operator_bool.md) | checks whether the reader holds a stream |

#### From mixin::reader

The rest of what a reader does, each an algorithm of io over this stream ([mixin::reader](../mixin/reader/README.md)).

| Function | Description |
|---|---|
| [read_full, async_read_full](../mixin/reader/read_full.md) | fills the whole buffer |
| [read_all, async_read_all](../mixin/reader/read_all.md) | everything to the end of the stream, as bytes |
| [read_all_text, async_read_all_text](../mixin/reader/read_all_text.md) | everything to the end of the stream, as a string |
| [copy_to, async_copy_to](../mixin/reader/copy_to.md) | the stream to its end, written to a writer |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | checks whether two readers hold the same stream |

## Complexity

A copy: three words. A call: one indirect call through the table, to the stream's own method.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

// A source of text kept as a field: any stream, chosen when the object is made
struct Words {
    io::reader source;

    size_t count() {
        auto text = source.read_all_text();
        return text ? vector<string_slice>(text->fields()).size() : 0;
    }
};

int main() {
    Words from_buffer{io::buffer("three short words")};

    int left = 2;
    Words from_lambda{[&](slice<byte> b) -> size_t {  // a reader too: says how much, 0 at the end
        if (left-- == 0) {
            return 0;
        }
        b[0] = byte('a');
        b[1] = byte(' ');
        return 2;
    }};

    println("{} {}", from_buffer.count(), from_lambda.count());
    println("{} {}", from_buffer.source.has_async_read(), from_lambda.source.has_async_read());
}
```

Output:

```text
3 2
true false
```

## See also

- [writer](../writer/README.md): any writer, as a value
- [req::reader](../req/reader.md), [req::async_reader](../req/reader.md): what a stream is
- [mixin::reader](../mixin/reader/README.md): the methods a reader has beside `read`
- [buffered_reader](../buffered_reader/README.md): lines and blocks over any reader
- [copy](../copy.md), [read_all](../read_all.md): the algorithms over any stream
