[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [address](README.md)

# sgcl::encoding::email::address::name

```cpp
const string& name() const noexcept;
```

The display name, decoded; `""` when there is none.

## Parameters

None.

## Return value

The text, a reference to the address's own.

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
    println("{}", encoding::email::address("=?utf-8?q?Zo=C3=AB?= <z@example.fr>").name());
}
```

Output:

```text
Zoë
```

## See also

- [to_string](to_string.md)
- [address](README.md)
