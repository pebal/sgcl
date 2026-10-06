[sgcl](../../README.md) › [crypto](../README.md) › [aes_cbc](README.md)

# sgcl::crypto::aes_cbc::encrypt

```cpp
vector<byte> encrypt(const slice<const byte>& plaintext);
```

Pads `plaintext` by PKCS #7 to the next whole block — 1 to 16 bytes, each the count of them, a whole block of 16 when
the message fills its last one — and encrypts it after what the object encrypted before (from the IV, for a fresh
one). The ciphertext is 1 to 16 bytes longer than the message.

## Parameters

| Parameter | Description |
|---|---|
| `plaintext` | the message, bytes or text, of any length |

## Return value

The ciphertext, the message's length rounded up to the next multiple of 16.

## Complexity

Linear in `plaintext.size()`.

## Exceptions

`logic_error` when the object was moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    vector<byte> key(16), iv(16);
    crypto::aes_cbc cbc(key, iv);
    println("{}", cbc.encrypt("").size());
    cbc.reset(iv);
    println("{}", cbc.encrypt("fifteen bytes!!").size());
    cbc.reset(iv);
    println("{}", cbc.encrypt("sixteen bytes!!!").size());
}
```

Output:

```text
16
16
32
```

## See also

- [decrypt](decrypt.md): the message back
- [encrypt_blocks](encrypt_blocks.md): whole blocks, no padding
- [sgcl::crypto::aes_cbc](README.md)
