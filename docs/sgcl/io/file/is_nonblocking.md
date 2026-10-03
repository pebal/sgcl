[sgcl](../../README.md) › [io](../README.md) › [file](README.md)

# sgcl::io::file::is_nonblocking

```cpp
bool is_nonblocking() const noexcept;
```

Checks whether the async operations of the file wait on the [reactor](../../async/readable.md) for readiness (a
non-blocking descriptor) rather than run on the [blocking pool](../../async/spawn_blocking.md). Chosen when the file is
made: [open](../open.md) makes a FIFO or a device non-blocking and leaves a regular file, a directory and a terminal
blocking; [pipe](../pipe.md) makes both ends non-blocking; [from_fd](../from_fd.md) takes the descriptor's flags as
they are.

## Parameters

None.

## Return value

`true` for a file served by the reactor, `false` for one served by the pool.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::file f = io::create("plain.txt");
    auto [in, out] = io::pipe().value();
    println("{} {} {}", f.is_nonblocking(), in.is_nonblocking(), out.is_nonblocking());
}
```

Output:

```text
false true true
```

## See also

- [read, async_read](read.md): what each kind of file waits on
- [sgcl::io::file](README.md)
