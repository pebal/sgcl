[sgcl](../../README.md) › [crypto](../README.md) › [hmac](README.md)

# sgcl::crypto::hmac\<H\>::operator=

```cpp
hmac& operator=(hmac&& other) noexcept;    // (1)
hmac& operator=(const hmac&) = delete;     // (2)
```

1. Takes over the states of `other`, its key and its message so far, in place of this hmac's own, and zeroes them in
   `other`, which gives no tag of any use until it is assigned again. An assignment to itself changes nothing.
2. No copy: the states are the key's equivalent. A second hmac under the same key is [clone](clone.md).

## Parameters

| Parameter | Description |
|---|---|
| `other` | the hmac moved from |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::hmac_sha256 mac("a key replaced");
    crypto::hmac_sha256 jefe("Jefe");
    mac = std::move(jefe);  // RFC 4231, test case 2
    mac.update("what do ya want for nothing?");
    println(encoding::hex::encode(mac.value()));
}
```

Output:

```text
5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843
```

## See also

- [(constructor)](hmac.md): the move constructor
- [clone](clone.md): a second hmac under the same key
- [sgcl::crypto::hmac\<H\>](README.md)
