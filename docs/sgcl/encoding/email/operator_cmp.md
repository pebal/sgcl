[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::operator== (sgcl::encoding::email)

```cpp
friend bool operator==(const email& a, const email& b) noexcept;
```

Whether `a` and `b` are handles of one message; two messages alike are not equal.

## Parameters

| Parameter | Description |
|---|---|
| `a, b` | the messages |

## Return value

`true` when both are the same message.

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
    encoding::email copy = m;
    encoding::email other = encoding::email::parse(m.to_string()).value();
    println("{} {}", copy == m, other == m);
}
```

Output:

```text
true false
```

## See also

- [email](README.md)
