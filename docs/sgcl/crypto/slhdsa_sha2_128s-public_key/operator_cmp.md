[sgcl](../../README.md) › [crypto](../README.md) › [slhdsa_sha2_128s](../slhdsa.md) › [public_key](README.md)

# sgcl::crypto::slhdsa_sha2_128s::operator==(public_key)

```cpp
friend bool operator==(const public_key& a, const public_key& b) noexcept;
```

Whether two keys have the same bytes.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the keys |

## Return value

`true` for the same bytes.

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
    auto key = crypto::slhdsa_sha2_128s::private_key::generate();
    println("{}", key.public_key() == key.clone().public_key());
}
```

Output:

```text
true
```

## See also

- [bytes](bytes.md)
- [sgcl::crypto::slhdsa_sha2_128s::public_key](README.md)
