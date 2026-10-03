[sgcl](../../README.md) › [io](../README.md) › [writer](README.md)

# sgcl::io::writer::fd

```cpp
int fd() const noexcept;
```

Returns the descriptor under the stream, for a stream that has one: a [file](../file/README.md), a standard stream, a
socket — a stream whose type has `fd()`. A child process takes such a stream as its descriptor, with no pipe and no
task between: `io::stdout` given as a command's output shares the program's own ([command](../command/README.md)).

## Parameters

None.

## Return value

The descriptor; -1 for a stream without one and for an empty writer.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::writer out = io::stdout;
    io::writer err = io::stderr;
    io::writer memory = io::buffer();
    println("{} {} {}", out.fd(), err.fd(), memory.fd());
}
```

Output:

```text
1 2 -1
```

## See also

- [file::fd](../file/fd.md): the descriptor of a file
- [sgcl::io::writer](README.md)
