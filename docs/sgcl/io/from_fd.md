[sgcl](../README.md) › [io](README.md)

# sgcl::io::from_fd

```cpp
#include "sgcl/io/file.h"   // or "sgcl/io.h"

namespace sgcl::io {
    file from_fd(int fd, const string& name = {}) noexcept;
}
```

A file over a descriptor opened elsewhere (a pipe from a child process, a socket, a descriptor a C library gave),
which the file owns from now on: [close](file/close.md), or the collector when the file is dead, closes it. The
descriptor's flags are left as they are: one that is non-blocking already is served by the
[reactor](../async/readable.md), any other by the [blocking pool](../async/spawn_blocking.md). Only `SIGPIPE` is
taken off: a write to a pipe whose read end is closed fails with `EPIPE` and does not end the process. Go's
`os.NewFile`.

## Parameters

| Parameter | Description |
|---|---|
| `fd` | an open descriptor, which nothing else closes from now on |
| `name` | the file's [path](file/path.md), which names it in the errors of its operations |

## Return value

The file over `fd`; nothing is checked of the descriptor.

## Complexity

Constant: one allocation and two system calls: the descriptor's flags read, and `SIGPIPE` taken off (on Linux an
`fstat(2)`, to see whether the writes must hold the signal back themselves).

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include <fcntl.h>
#include <unistd.h>

using namespace sgcl;

int main() {
    int fd = ::open("raw.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    io::file f = io::from_fd(fd, "raw.txt");
    f.write("written through the file");
    f.close();
    println("{} {}", f.is_nonblocking(), *io::read_text("raw.txt"));

    int ends[2];
    ::pipe(ends);
    ::fcntl(ends[0], F_SETFL, O_NONBLOCK);
    io::file in = io::from_fd(ends[0]);
    io::file out = io::from_fd(ends[1], "out");
    println("{} {} '{}' {}", in.is_nonblocking(), out.is_nonblocking(), in.path(), out.path());
}
```

Output:

```text
false written through the file
true false '' out
```

## See also

- [fd](file/fd.md): the descriptor of a file
- [open](open.md): a file opened by the library
- [sgcl::io::file](file.md)
