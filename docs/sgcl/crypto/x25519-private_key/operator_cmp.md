[sgcl](../../README.md) › [crypto](../README.md) › [x25519](../x25519.md) › [private_key](../x25519-private_key.md)

# sgcl::crypto::x25519::operator== (sgcl::crypto::x25519::private_key)

```cpp
friend bool operator==(const private_key& a, const private_key& b);
```

Compares the secret bytes of two keys in constant time, as [constant_time::equal](../constant_time/equal.md) does.
The bytes are compared as given, not clamped: two keys that differ only in the bits clamping sets have the same
public key but are not equal. `!=` is made from it by the compiler.

## Parameters

| Parameter | Description |
|---|---|
| `a`, `b` | the keys compared |

## Return value

Whether the keys hold the same 32 bytes.

## Complexity

Constant.

## Exceptions

`logic_error` when either key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::x25519::private_key::generate();
    auto other = crypto::x25519::private_key::generate();
    println("{} {}", key == key.clone(), key == other);
}
```

Output:

```text
true false
```

## See also

- [clone](clone.md): a second key of the same bytes
- [sgcl::crypto::x25519::private_key](../x25519-private_key.md)
