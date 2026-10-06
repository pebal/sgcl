[sgcl](../../README.md) › [encoding](../README.md) › [dotenv](README.md)

# sgcl::encoding::operator== (sgcl::encoding::dotenv)

```cpp
friend bool operator==(const dotenv& a, const dotenv& b) noexcept;
```

Whether the entries are the same keys with the same values in the same order; `!=` is made from it by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the entries |

## Return value

`true` when they are equal.

## Complexity

Linear in the size of the entries.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto p = [](const char* t) { return encoding::dotenv::parse(t).value(); };
    println("{} {} {}", p("A=1\nB=2") == p("A='1'\nB=\"2\""), p("A=1\nB=2") == p("B=2\nA=1"), p("A=1") == p("A=1 # c"));
}
```

Output:

```text
true false true
```

## See also

- [members](members.md)
- [sgcl::encoding::dotenv](README.md)
