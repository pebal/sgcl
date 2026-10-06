[sgcl](../../README.md) › [io](../README.md) › [terminal_mode](README.md)

# sgcl::io::terminal_mode::terminal_mode

```cpp
terminal_mode() noexcept = default;               // (1)
terminal_mode(terminal_mode&& other) noexcept;    // (2)
terminal_mode(const terminal_mode&) = delete;     // (3)
```

1. A guard that holds no terminal: `!raw`, its restore does nothing.
2. The guard of `other`'s terminal, `other` left holding none.
3. Not copied: one guard restores a terminal.

A guard of a terminal is made by [make_raw](../make_raw.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the guard whose terminal this one takes over |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::terminal_mode none;
    println("{}", static_cast<bool>(none));
    io::pty term = io::open_pty().value();
    io::terminal_mode raw = io::make_raw(term.terminal().fd()).value();
    io::terminal_mode taken = std::move(raw);
    println("{} {}", static_cast<bool>(raw), static_cast<bool>(taken));
}
```

Output:

```text
false
false true
```

## See also

- [make_raw](../make_raw.md): what makes one
- [sgcl::io::terminal_mode](README.md)
