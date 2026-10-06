[sgcl](../../README.md) › [io](../README.md) › [pty](README.md)

# sgcl::io::pty::pty

```cpp
pty() noexcept = default;          // (1)
pty(const pty& other) noexcept;    // (2), implicitly declared
pty(pty&& other) noexcept;         // (3), implicitly declared
```

1. A handle that holds no pseudo-terminal: `!p`. An operation on it is a contract violation (debug builds assert);
   it is given one by an assignment.
2. A handle of the same pseudo-terminal as `other`: one master, one terminal end, shared.
3. The same; `other` keeps holding it, a move of the word being a copy.

A pseudo-terminal is made by [open_pty](../open_pty.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the handle whose pseudo-terminal this one shares |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::pty none;
    println("{}", static_cast<bool>(none));
    io::pty term = io::open_pty().value();
    io::pty same = term;
    same.close();
    println("{} {}", term.is_closed(), same == term);
}
```

Output:

```text
false
true true
```

## See also

- [open_pty](../open_pty.md): what makes one
- [operator bool](operator_bool.md): whether the handle holds one
- [sgcl::io::pty](README.md)
