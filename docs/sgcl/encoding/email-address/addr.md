[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [address](README.md)

# sgcl::encoding::email::address::addr

```cpp
const string& addr() const noexcept;
```

The addr-spec, `alice@example.com`.

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
    println("{}", encoding::email::address("Alice <alice@example.com>").addr());
}
```

Output:

```text
alice@example.com
```

## See also

- [to_string](to_string.md)
- [address](README.md)
