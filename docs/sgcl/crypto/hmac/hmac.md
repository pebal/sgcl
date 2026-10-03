[sgcl](../../README.md) › [crypto](../README.md) › [hmac](../hmac.md)

# sgcl::crypto::hmac\<H\>::hmac

```cpp
/*(1)*/ explicit hmac(const slice<const byte>& key) noexcept;
/*(2)*/ hmac(hmac&& other) noexcept;
/*(3)*/ hmac(const hmac&) = delete;
```

1. Makes an hmac under `key`, ready for a message. The key is bytes or text of any length: one longer than the
   digest's block is hashed first, a shorter one padded with zeros (RFC 2104 §2). An empty key is allowed, as the
   standard has it, and is no secret. The key is not kept: what is kept is the digest's state after the key XOR ipad
   (where every message starts) and after the key XOR opad, and the buffers the pads were made in are zeroed before
   the constructor returns.
2. Takes over the states of `other` and zeroes them in `other`, which gives no tag of any use until it is assigned
   again.
3. No copy: the states are the key's equivalent. A second hmac under the same key, at the same point of its message,
   is [clone](clone.md).

## Parameters

| Parameter | Description |
|---|---|
| `key` | the secret key, bytes or text |
| `other` | the hmac moved from |

## Complexity

- (1) Linear in the key's length past a block; otherwise two compressions, one for each pad.
- (2) Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 4231, test case 6: a key of 131 bytes, longer than the block, hashed first
    vector<byte> key(131, byte(0xaa));
    crypto::hmac_sha256 mac(key);
    mac.update("Test Using Larger Than Block-Size Key - Hash Key First");

    crypto::hmac_sha256 moved = std::move(mac);  // mac is zeroed
    println(encoding::hex::encode(moved.value()));
}
```

Output:

```text
60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54
```

## See also

- [operator=](operator_assign.md): the move assignment
- [clone](clone.md): a second hmac under the same key
- [sgcl::crypto::hmac\<H\>](../hmac.md)
