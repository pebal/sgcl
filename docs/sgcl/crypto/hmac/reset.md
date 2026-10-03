[sgcl](../../README.md) › [crypto](../README.md) › [hmac](README.md)

# sgcl::crypto::hmac\<H\>::reset

```cpp
void reset() noexcept;
```

Puts the hmac back to what the constructor made with its key: the message dropped, the key kept. The inner state goes
back to the one the key's inner pad left, so one hmac tags message after message without the key being given or
hashed again.

## Parameters

None.

## Return value

None.

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
    crypto::hmac<crypto::sha384> mac("Jefe");
    mac.update("an earlier message");
    mac.reset();
    mac.update("what do ya want for nothing?");  // RFC 4231, test case 2
    println(encoding::hex::encode(mac.value()));
}
```

Output:

```text
af45d2e376484031617f78d2b58a6b1b9c7ef464f5a01b47e42ec3736322445e8e2240ca5e69e2c78b3239ecfab21649
```

## See also

- [(constructor)](hmac.md): an hmac under a key
- [clone](clone.md): a second hmac at the same point
- [sgcl::crypto::hmac\<H\>](README.md)
