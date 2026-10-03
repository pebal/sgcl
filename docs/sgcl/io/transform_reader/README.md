[sgcl](../../README.md) › [io](../README.md)

# sgcl::io::transform_reader\<F\>

```cpp
#include "sgcl/io/stream.h"   // or "sgcl/io.h"

namespace sgcl::io {
    template<class F>
    class transform_reader;   // : public mixin::reader<transform_reader<F>>
}
```

`sgcl::io::transform_reader<F>` is a reader that hands on what another reader reads after a function has changed it
in place: `f` is called with the bytes just read, as a `slice<byte>`, and may change them but not their number. A
decryption with a stream cipher, a case or a byte mapping, a count of what passes: the most common wrapper of a
stream, in a line, without a class of its own. Both forms are there, each over the source's own. Go has no
counterpart in `io` (a `cipher.StreamReader` is one case of it); neither has the standard library.

## Rules

- It holds an [io::reader](../reader/README.md), a `tracked_ptr`, and the function, so it lives where a `tracked_ptr` may: on a
  stack, in a task, in a managed object ([The rules](../../core/README.md#the-rules), 1); a function that captures a
  tracked pointer is kept with it.
- An object, not a handle: given by reference to an `io::reader` or to `io::copy`, it is referenced, and the caller
  keeps it alive; given as a temporary, it is copied into a managed object of its own.
- [async_read](read.md) is over the source's own `async_read`; for a source that has only `read`,
  the source's `io::reader` runs it on the [blocking pool](../../async/spawn_blocking.md). The function runs on the task's
  thread, after the read.
- One thread or task at a time, as on any stream.

## Template parameters

| Parameter | Description |
|---|---|
| `F` | The function: callable as `f(slice<byte>)` with the bytes just read, its result ignored. Moved into the reader. |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](transform_reader.md) | constructs the reader over a source and a function |
| [read, async_read](read.md) | reads bytes of the source and gives them to the function |

#### From mixin::reader

The rest of what a reader does, each an algorithm of io over this stream ([mixin::reader](../mixin/reader/README.md)).

| Function | Description |
|---|---|
| [read_full, async_read_full](../mixin/reader/read_full.md) | fills the whole buffer |
| [read_all, async_read_all](../mixin/reader/read_all.md) | everything to the end of the source, as bytes |
| [read_all_text, async_read_all_text](../mixin/reader/read_all_text.md) | everything to the end of the source, as a string |
| [copy_to, async_copy_to](../mixin/reader/copy_to.md) | the source to its end, written to a writer |

## Deduction guides

```cpp
template<class F>
transform_reader(reader, F) -> transform_reader<F>;
```

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer secret("Ifmmp!uifsf\n");  // each character one above its own
    io::transform_reader plain(secret, [](slice<byte> bytes) {
        for (byte& b : bytes) {
            if (b > byte(' ')) {
                b = byte(int(b) - 1);
            }
        }
    });
    io::copy(io::stdout, plain);
}
```

Output:

```text
Hello there
```

## See also

- [limit_reader](../limit_reader/README.md), [multi_reader](../multi_reader/README.md), [tee_reader](../tee_reader/README.md): the other readers
  over readers
- [reader](../reader/README.md)
