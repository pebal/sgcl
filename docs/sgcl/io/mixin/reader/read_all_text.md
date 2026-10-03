[sgcl](../../../README.md) › [io](../../README.md) › [mixin](../README.md) › [reader](README.md)

# sgcl::io::mixin::reader\<Derived\>::read_all_text, async_read_all_text

```cpp
expected<string, error> read_all_text();                               // (1)
async::task<expected<string, error>> async_read_all_text() noexcept    // (2)
    requires req::async_reader<Derived&>;
```

Reads this stream to its end and returns its bytes as a [string](../../../core/string/README.md), taken as they are. It is
[io::read_all_text](../../read_all_text.md) over this stream: the bytes gathered, then one string of their size.

1. Reads on the calling thread.
2. The same for a task, with `Derived`'s `async_read`; takes part only when `Derived` has it. The stream is held by
   reference while the task runs: the caller keeps it alive until the task is done.

## Parameters

None.

## Return value

The text from the position to the end, the empty string for a stream at its end, or the error of a read, as the
stream gave it; the bytes read before it are dropped.

## Complexity

Linear in the number of bytes read.

## Exceptions

- (1) `length_error` when the text passes 4 294 967 295 bytes, the most a string holds; what `Derived`'s `read`
  throws.
- (2) None. What is thrown while it runs is the task's: its `co_await` rethrows it.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::buffered_reader in(io::buffer("Subject: hello\n\nThe body,\nto the end.\n"));
    while (auto line = in.read_line()) {
        if (!*line || (*line)->empty()) {
            break;  // the end of the header
        }
        println("header: {}", **line);
    }
    print("body:\n{}", *in.read_all_text());
}
```

Output:

```text
header: Subject: hello
body:
The body,
to the end.
```

## See also

- [io::read_all_text](../../read_all_text.md): the same over any stream
- [read_all](read_all.md): the same as bytes
- [sgcl::io::mixin::reader\<Derived\>](README.md)
