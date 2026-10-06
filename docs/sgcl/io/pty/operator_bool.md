[sgcl](../../README.md) › [io](../README.md) › [pty](README.md)

# sgcl::io::pty::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether the handle holds a pseudo-terminal. A handle made by the default constructor holds none; one moved
from still holds it, a move of the word being a copy. A closed pseudo-terminal is still held
([is_closed](is_closed.md) tells it apart).

## Parameters

None.

## Return value

`true` when the handle holds a pseudo-terminal, `false` otherwise.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::pty term;
    println("{}", static_cast<bool>(term));
    term = io::open_pty().value();
    term.close();
    println("{}", static_cast<bool>(term));
}
```

Output:

```text
false
true
```

## See also

- [(constructor)](pty.md): a handle with none
- [sgcl::io::pty](README.md)
