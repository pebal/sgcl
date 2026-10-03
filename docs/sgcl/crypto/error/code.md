[sgcl](../../README.md) › [crypto](../README.md) › [error](../error.md)

# sgcl::crypto::error::code

```cpp
errc code() const noexcept;
```

Returns what went wrong, one of [errc](../errc.md): what a program reads with a `switch` to tell a forged message
from a key that cannot be one or from DER that cannot be read.

## Parameters

None.

## Return value

The code.

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
    // RFC 8032's TEST 1 key; the bytes of a point of the curve are refused
    // when they are not its canonical encoding
    for (const char* hex : {"d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a",
                            "edffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f",
                            "d75a98"}) {
        auto key = crypto::ed25519::public_key::from_bytes(encoding::hex::decode(hex));
        if (key) {
            println("a key");
            continue;
        }
        switch (key.error().code()) {
            case crypto::errc::invalid_key:
                println("not a key: {}", key.error().message());
                break;
            default:
                println("another error");
        }
    }
}
```

Output:

```text
a key
not a key: not the encoding of a point of Ed25519
not a key: an Ed25519 public key is 32 bytes
```

## See also

- [errc](../errc.md): the codes
- [message](message.md): the error as a text
- [sgcl::crypto::error](../error.md)
