[sgcl](../../README.md) › [crypto](../README.md) › [hmac](../hmac.md)

# sgcl::crypto::hmac\<H\>::digest

```cpp
array<byte, digest_size> digest() const noexcept;
```

The tag of the message so far, the same bytes as [value](value.md): the name every hasher has for its result as
bytes, so that code over any hasher (`hash::req::hasher`) reads an hmac's tag one way.

## Parameters

None.

## Return value

The tag, `H::digest_size` bytes.

## Complexity

Constant: the end of the inner digest and the outer one over its result.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/hash.h"
#include "sgcl/io.h"

using namespace sgcl;

// Any hasher, by reference: an hmac is not copied
string hex_of(hash::req::hasher auto& h, const string& text) {
    h.update(text);
    return encoding::hex::encode(h.digest());
}

int main() {
    crypto::hmac<crypto::sha224> mac("Jefe");  // RFC 4231, test case 2
    println(hex_of(mac, "what do ya want for nothing?"));
}
```

Output:

```text
a30e01098bc6dbbf45690f3a7e9e6d0f8bbea2a39e6148008fd05e44
```

## See also

- [value](value.md): the same bytes
- [hash::mixin::hasher](../../hash/mixin/hasher.md): the shape every hasher shares
- [sgcl::crypto::hmac\<H\>](../hmac.md)
