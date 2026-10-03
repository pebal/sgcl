[sgcl](../../README.md) › [slog](../README.md) › [handler](README.md)

# sgcl::slog::handler::operator bool

```cpp
explicit operator bool() const noexcept;
```

Checks whether a handler is held: `false` for a default-constructed one, which is what
[options](../options.md)`::handler` is when the lines go to `out`.

## Parameters

None.

## Return value

`true` when a handler is held.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/slog.h"

using namespace sgcl;

int main() {
    slog::options o;
    slog::memory kept;
    slog::handler h = kept;
    println("{} {}", bool(o.handler), bool(h));
}
```

Output:

```text
false true
```

## See also

- [(constructor)](handler.md)
- [sgcl::slog::handler](README.md)
