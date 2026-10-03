[sgcl](../../README.md) › [crypto](../README.md) › [ed25519](../ed25519.md) › [public_key](../ed25519-public_key.md)

# sgcl::crypto::ed25519::public_key::verify

```cpp
[[nodiscard]] bool verify(const slice<const byte>& message,
                          const slice<const byte>& signature) const noexcept;
```

Checks whether `signature` is this key's signature of `message`, RFC 8032 §5.1.7 as Go's `ed25519.Verify` checks it:
S must be below the group order L; k = SHA-512(R ‖ A ‖ M) mod L; and the encoding of [S]B − [k]A must be R, compared
as bytes, so a non-canonical R never matches. The equation is the one without the cofactor. The message is bytes or
text: `verify("text", signature)` verifies the text's bytes.

Everything here is public — the key, the message, the signature — and the verification takes branches on it, as
every implementation's does.

## Parameters

| Parameter | Description |
|---|---|
| `message` | the message signed, bytes or text |
| `signature` | the signature, 64 bytes: R and S |

## Return value

`true` when the signature is the key's signature of the message; `false` otherwise, and for a signature of another
length than 64.

## Complexity

Linear in the size of `message`, and one double-scalar multiplication.

## Exceptions

None.

## Notes

`[[nodiscard]]`: a check whose result is dropped was never made, and the compiler warns. Under a key of small order a
signature can be had for nothing, and such keys are not refused, as neither the RFC nor Go refuses them: a protocol
that must bind a signature to one signer checks the key where it is registered.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // RFC 8032's TEST 2: the public key, the message 0x72 and its signature
    auto key = crypto::ed25519::public_key::from_bytes(
        encoding::hex::decode("3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c"));
    vector<byte> signature = encoding::hex::decode(
        "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da"
        "085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00");
    println("{}", key->verify(encoding::hex::decode("72"), signature));
    println("{}", key->verify("r", signature));  // the same byte, as text

    signature[63] ^= byte{0x80};  // S past L
    println("{}", key->verify("r", signature));
    println("{}", key->verify("r", signature.as_slice().subslice(0, 63)));
}
```

Output:

```text
true
true
false
false
```

## See also

- [ed25519::private_key::sign](../ed25519-private_key/sign.md): the signature
- [sgcl::crypto::ed25519::public_key](../ed25519-public_key.md)
