[sgcl](../../README.md) › [crypto](../README.md) › [mldsa65](../mldsa.md) › [public_key](README.md)

# sgcl::crypto::mldsa65::operator==(public_key)

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

Linear in the size of the key.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::mldsa65::private_key::generate();
    println("{}", key.public_key() == key.clone().public_key());
}
```

Output:

```text
true
```

## See also

- [bytes](bytes.md)
- [sgcl::crypto::mldsa65::public_key](README.md)
