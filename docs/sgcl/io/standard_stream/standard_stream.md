[sgcl](../../README.md) › [io](../README.md) › [standard_stream](README.md)

# sgcl::io::standard_stream::standard_stream

```cpp
constexpr standard_stream(int fd, const char* name) noexcept;    // (1)
standard_stream(const standard_stream&) = delete;                // (2)
```

1. A stream over the descriptor `fd`, named `name` in its errors. Nothing is made and nothing is checked: the file
   over the descriptor is made by the first operation, so the constructor is a constant expression, and the three
   objects `io::stdin`, `io::stdout` and `io::stderr` are initialized before any code of the program runs.
2. Deleted: a stream is not copied, nor moved. The assignment is deleted as well.

## Parameters

| Parameter | Description |
|---|---|
| `fd` | the descriptor, open for the life of the stream |
| `name` | the name of the stream in its errors, a string that lives as long as the stream (a literal) |

## Complexity

Constant.

## Exceptions

None.

## Notes

The three objects of the module are what a program uses; a stream of its own over another descriptor the process
inherited (3, given by a shell's `3>`) is a [file](../file/README.md) made by [from_fd](../from_fd.md), which may be
closed.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    println("{} {} {}", io::stdin.fd(), io::stdout.fd(), io::stderr.fd());
}
```

Output:

```text
0 1 2
```

## See also

- [fd](fd.md): the descriptor
- [from_fd](../from_fd.md): a file over a descriptor the process has
- [sgcl::io::standard_stream](README.md)
