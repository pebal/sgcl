[sgcl](../../README.md) › [crypto](../README.md) › [ed25519](../ed25519.md) › [private_key](README.md)

# sgcl::crypto::ed25519::private_key::public_key

```cpp
ed25519::public_key public_key() const;
```

Returns the public key, the one that verifies the key's signatures. It was computed when the key was made, A = s·B
by the constant-time fixed-base multiplication, and is kept decoded; this is a copy.

## Parameters

None.

## Return value

The [public key](../ed25519-public_key/README.md).

## Complexity

Constant.

## Exceptions

`logic_error` when the key was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 8032's TEST 1 key
    auto key = crypto::ed25519::private_key::from_seed(
        encoding::hex::decode("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60"));
    crypto::ed25519::public_key published = key->public_key();
    println("{}", encoding::hex::encode(published.bytes()));
    println("{}", published.verify("a message", key->sign("a message")));
}
```

Output:

```text
d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a
true
```

## See also

- [sign](sign.md): what it verifies
- [ed25519::public_key](../ed25519-public_key/README.md)
- [sgcl::crypto::ed25519::private_key](README.md)
