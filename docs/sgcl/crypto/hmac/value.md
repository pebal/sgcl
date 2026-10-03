[sgcl](../../README.md) › [crypto](../README.md) › [hmac](README.md)

# sgcl::crypto::hmac\<H\>::value

```cpp
array<byte, digest_size> value() const noexcept;
```

The tag of the message so far, `H(key ^ opad || H(key ^ ipad || message))`. It is computed on copies of the inner and
the outer state, which are zeroed before it returns, so the hmac goes on: more `update` after `value()` gives the tag
of the longer message. The tag itself is returned by value and is not zeroed: it is a copy on the caller's stack. A
received tag is checked with [verify](verify.md), not by comparing it with this.

## Parameters

None.

## Return value

The tag, `H::digest_size` bytes: 32 for `hmac_sha256`, 64 for `hmac_sha512`.

## Complexity

Constant: the end of the inner digest and the outer one over its result.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 4231, test case 2
    crypto::hmac_sha512 mac("Jefe");
    mac.update("what do ya want ");
    auto prefix = mac.value();  // the hmac goes on
    mac.update("for nothing?");
    println(encoding::hex::encode(mac.value()));
    println("{}", prefix.size());
}
```

Output:

```text
164b7a7bfcf819e2e395fbe73b56e0a387bd64222e831fd610270cd7ea2505549758bf75c05a994a6d034f65f8f0e6fdcaeab1a34d4a6b4b636e070a38bce737
64
```

## See also

- [verify](verify.md): checks a received tag
- [digest](digest.md): the same bytes, under the name every hasher has
- [sgcl::crypto::hmac\<H\>](README.md)
