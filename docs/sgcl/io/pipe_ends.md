[sgcl](../README.md) › [io](README.md)

# sgcl::io::pipe_ends

```cpp
#include "sgcl/io/file.h"   // or "sgcl/io.h"

namespace sgcl::io {
    struct pipe_ends {
        file read;
        file write;
    };
}
```

`sgcl::io::pipe_ends` is the two ends of a pipe, by name, as [pipe](pipe.md) returns them: `ends.read` and
`ends.write`, or `auto [r, w] = io::pipe().value();`. Each is a [file](file/README.md), a handle: copied into a task, it
is the same end.

## Rules

- The struct holds two handles, tracked words: it lives where a [file](file/README.md) may, on a stack, in a task, in a
  managed object.

## Member objects

| Field | Description |
|---|---|
| `read` | the read end: what is written to the other end is read here |
| `write` | the write end |

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::pipe_ends ends = io::pipe().value();
    ends.write.write("through the pipe");
    ends.write.close();
    println("{}", *ends.read.read_all_text());
}
```

Output:

```text
through the pipe
```

## See also

- [pipe](pipe.md): what makes one
- [sgcl::io::file](file/README.md)
