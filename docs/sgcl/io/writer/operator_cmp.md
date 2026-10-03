[sgcl](../../README.md) › [io](../README.md) › [writer](../writer.md)

# sgcl::io::operator== (sgcl::io::writer)

```cpp
friend bool operator==(const writer& a, const writer& b) noexcept;
```

Checks whether `a` and `b` hold the same stream: the same object. Two writers of `io::stdout` are equal, and so are
two made of copies of one handle; two writers of two buffers are not. Two empty writers are equal. `!=` is the
negation. A child process compares its output and its error streams so: the same writer in both gets one pipe
([command](../command.md)).

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the writers to compare |

## Return value

`true` when both hold the same stream, or both are empty.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    io::writer out = io::stdout;
    io::writer again = io::stdout;
    io::buffer kept;
    io::writer a = kept;
    io::writer b = io::buffer();
    println("{} {} {}", out == again, a == b, a == io::writer(kept));
}
```

Output:

```text
true false true
```

## See also

- [sgcl::io::writer](../writer.md)
