[sgcl](../../README.md) › [slog](../README.md) › [rotating_file](README.md)

# sgcl::slog::operator==, operator!= (sgcl::slog::rotating_file)

```cpp
friend bool operator==(const rotating_file& a, const rotating_file& b) noexcept;
```

Checks whether two handles stand for the same file: one a copy of the other, or both copies of one. Two opens of
one path are two files. The `!=` is the one C++ writes from this `==`.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the handles to compare |

## Return value

`true` when `a` and `b` are the same.

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
    slog::rotating_file a = slog::rotating_file::open("app.log");
    slog::rotating_file b = a;
    slog::rotating_file c = slog::rotating_file::open("other.log");
    println("{} {}", a == b, a == c);
}
```

Output:

```text
true false
```

## See also

- [(constructor)](rotating_file.md)
- [sgcl::slog::rotating_file](README.md)
