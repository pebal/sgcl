[sgcl](../../../README.md) › [crypto](../../README.md) › [aead](../aead.md)

# sgcl::crypto::mixin::aead\<Derived\>::open

```cpp
/*(1)*/ [[nodiscard]] expected<vector<byte>, error> open(const slice<const byte>& nonce,
                                                         const slice<const byte>& sealed) const;
/*(2)*/ [[nodiscard]] expected<vector<byte>, error> open(const slice<const byte>& nonce,
                                                         const slice<const byte>& sealed,
                                                         const slice<const byte>& aad) const;
```

Checks the tag at the end of `sealed` against the ciphertext before it, the nonce and the additional data, and
when it matches, decrypts the ciphertext: Go's `aead.Open(nil, nonce, sealed, aad)`.

1. Without additional data.
2. With `aad`, the additional data the sender gave to [seal](seal.md).

The tag is computed over the ciphertext and compared in constant time before the first byte is decrypted: a
forgery is never decrypted, and nothing of it reaches the vector.

## Parameters

| Parameter | Description |
|---|---|
| `nonce` | the nonce the data was sealed with, exactly `nonce_size` bytes of the class |
| `sealed` | the ciphertext followed by the tag, as [seal](seal.md) gives it |
| `aad` | the additional data the data was sealed with |

## Return value

The plaintext, `sealed.size() - tag_size` bytes, or an [error](../../error.md) of
[errc::authentication](../../errc.md) ("message authentication failed") when the tag does not match (the data, the
tag, the nonce, the additional data or the key is not the one sealed), when `sealed` is shorter than a tag and when
it is longer than `max_plaintext_size + tag_size`. The error says nothing more.

## Complexity

Linear in the sizes of `sealed` and `aad`: two passes over the ciphertext when the tag matches, the tag and then
the decryption.

## Exceptions

- `invalid_argument` when `nonce` is not `nonce_size` bytes.
- `logic_error` when the object was moved from and holds no key.

## Notes

The plaintext is the user's data, not key material: a managed `vector<byte>`, freed by the collector in its time
and zeroed by nobody. A plaintext that must not stay in memory, a key unwrapped or a password, is opened with
[open_to](open_to.md) into a [secret_bytes](../../secret_bytes.md).

The result is `[[nodiscard]]`: a verification whose result is dropped is a hole, and the compiler says so.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // The test case 2 of the GCM specification: a zero key, a zero nonce, 16 zero bytes
    crypto::aes_gcm gcm(vector<byte>(16));
    vector<byte> nonce(12);
    vector<byte> sealed = encoding::hex::decode(
        "0388dace60b6a392f328c2b971b2fe78ab6e47d42cec13bdf53a67b21257bddf");

    auto opened = gcm.open(nonce, sealed);
    println("{}", encoding::hex::encode(*opened));

    string header = "to: hq";
    auto with_other_aad = gcm.open(nonce, sealed, header);
    println("{}", with_other_aad.error().message());

    sealed[31] ^= byte(1);  // one bit of the tag changed on the way
    auto forged = gcm.open(nonce, sealed);
    println("{}", forged.error().message());

    auto too_short = gcm.open(nonce, sealed.as_slice(0, 10));
    println("{}", too_short.has_value());
}
```

Output:

```text
00000000000000000000000000000000
message authentication failed
message authentication failed
false
```

## See also

- [seal](seal.md): encrypts and authenticates
- [open_to](open_to.md): into the caller's buffer, in place too
- [xchacha20_poly1305::open_random](../../xchacha20_poly1305/open_random.md): the nonce read from the front
- [sgcl::crypto::mixin::aead\<Derived\>](../aead.md)
