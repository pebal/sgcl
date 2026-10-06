[sgcl](../../README.md) › [io](../README.md) › [terminal_mode](README.md)

# sgcl::io::terminal_mode::operator=

```cpp
terminal_mode& operator=(terminal_mode&& other) noexcept;    // (1)
terminal_mode& operator=(const terminal_mode&) = delete;     // (2)
```

1. Restores the terminal this guard holds, if any, and takes over `other`'s, `other` left holding none. An
   assignment of a guard to itself does nothing.
2. Not copied: one guard restores a terminal.

## Parameters

| Parameter | Description |
|---|---|
| `other` | the guard whose terminal this one takes over |

## Return value

`*this`.

## Complexity

Constant: one system call when this guard held a terminal.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::pty term = io::open_pty().value();
    io::terminal_mode raw = io::make_raw(term.terminal().fd()).value();
    raw = io::terminal_mode();  // restores the terminal
    term.write("cooked\n");
    byte line[16];
    size_t n = term.terminal().read(line).value();
    println("{} bytes", n);
}
```

Output:

```text
7 bytes
```

## See also

- [restore](restore.md): the same without a new guard
- [sgcl::io::terminal_mode](README.md)
