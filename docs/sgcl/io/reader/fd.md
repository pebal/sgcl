[sgcl](../../README.md) › [io](../README.md) › [reader](README.md)

# sgcl::io::reader::fd

```cpp
int fd() const noexcept;
```

Returns the descriptor under the stream, for a stream that has one: a [file](../file/README.md), a standard stream, a
socket — a stream whose type has `fd()`. A child process takes such a stream as its descriptor, with no pipe and no
task between ([command](../command/README.md)).

## Parameters

None.

## Return value

The descriptor; -1 for a stream without one and for an empty reader.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::reader input = io::stdin;
    io::reader memory = io::buffer("in memory");
    println("{} {} {}", input.fd(), memory.fd(), io::reader().fd());
}
```

Output:

```text
0 -1 -1
```

## See also

- [file::fd](../file/fd.md): the descriptor of a file
- [sgcl::io::reader](README.md)
