[sgcl](../README.md) › [io](README.md)

# sgcl::io::read_all_text, async_read_all_text

```cpp
#include "sgcl/io/functions.h"   // or "sgcl/io.h"

namespace sgcl::io {
    template<req::reader R>
    expected<string, error> read_all_text(R&& r);                      // (1)
    template<req::async_reader R>
    async::task<expected<string, error>> async_read_all_text(R&& r)    // (2)
        noexcept(/* see below */);
}
```

Reads the stream `r` to its end and returns its bytes as a [string](../core/string.md), taken as they are: the text
is UTF-8 by convention and nothing is checked or converted.

1. Reads on the calling thread, with the stream's `read`. The bytes are gathered in unmanaged memory, as
   [read_all](read_all.md) gathers them, and copied once into a string of their size: one managed allocation, the
   result, and no vector between.
2. The same for a task, with the stream's `async_read`: read as [async_read_all](read_all.md) reads, into managed
   blocks, then made a string. A stream given as a temporary is moved into the task's frame; one given by reference
   is the caller's to keep alive until the task is done.

`r` is any [reader](req/reader.md) (2: async reader). A class that carries
[mixin::reader](mixin/reader.md) has the same as a member, [r.read_all_text()](mixin/reader/read_all_text.md).

## Parameters

| Parameter | Description |
|---|---|
| `r` | the stream to read |

## Return value

The text of the stream from its position to its end, the empty string for a stream at its end, or the error of a
read of `r`, as it gave it; the bytes read before it are dropped.

## Complexity

Linear in the number of bytes read.

## Exceptions

- (1) `length_error` when the text passes 4 294 967 295 bytes, the most a string holds; what the read of `r` throws.
- (2) What the move of a stream given as a temporary into the task's frame throws; none for a stream given by
  reference. What is thrown while it runs, a read's exception or the string's `length_error`, is the task's: its
  `co_await` rethrows it.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffer greeting("hello, text");
    auto text = io::read_all_text(greeting);
    println("{} ({} characters)", *text, text->size());
    println("'{}'", *io::read_all_text(greeting));  // at its end: the empty string

    auto later = io::async_read_all_text(io::buffer("from a task")).wait();
    println("{}", *later);
}
```

Output:

```text
hello, text (11 characters)
''
from a task
```

## See also

- [read_all](read_all.md): the same as bytes
- [mixin::reader::read_all_text](mixin/reader/read_all_text.md): the same as a member of a stream
- [read_text](read_text.md): a whole file by its path
- [string](../core/string.md)
