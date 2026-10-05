[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [part](README.md)

# sgcl::encoding::operator== (sgcl::encoding::email::part)

```cpp
friend bool operator==(const part& a, const part& b) noexcept;
```

Whether `a` and `b` are handles of one part.

## Parameters

| Parameter | Description |
|---|---|
| `a, b` | the parts |

## Return value

`true` when both are the same part.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    encoding::email m("a@example.com", "b@example.com", "s", "t");
    println("{} {}", m.body() == m.body(), m.body() == encoding::email::part("text/plain", "t"));
}
```

Output:

```text
true false
```

## See also

- [part](README.md)
