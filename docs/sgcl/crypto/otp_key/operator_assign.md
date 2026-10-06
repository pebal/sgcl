[sgcl](../../README.md) › [crypto](../README.md) › [otp_key](README.md)

# sgcl::crypto::otp_key::operator=

```cpp
otp_key& operator=(otp_key&& other) noexcept;    // (1)
otp_key& operator=(const otp_key&) = delete;     // (2)
```

1. Takes the key of `other` over, its secret zeroing the one this key held; `other`'s secret is left empty.
2. The secret is not copied by accident: a copy is [clone](clone.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the key taken over |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::otp_key key;
    key = crypto::otp_key::generate("Example", "alice");
    println("{} {}", key.account, key.secret.size());
}
```

Output:

```text
alice 20
```

## See also

- [clone](clone.md): a copy made on purpose
- [sgcl::crypto::otp_key](README.md)
