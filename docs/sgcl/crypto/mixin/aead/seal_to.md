[sgcl](../../../README.md) › [crypto](../../README.md) › [aead](../aead.md)

# sgcl::crypto::mixin::aead\<Derived\>::seal_to

```cpp
size_t seal_to(const slice<byte>& out, const slice<const byte>& nonce,                     // (1)
               const slice<const byte>& plaintext) const;
size_t seal_to(const slice<byte>& out, const slice<const byte>& nonce,                     // (2)
               const slice<const byte>& plaintext, const slice<const byte>& aad) const;
```

Seals as [seal](seal.md) does, into a buffer the caller gives, with nothing allocated: the ciphertext and the tag
are written to the first `plaintext.size() + tag_size` bytes of `out`, and the rest of `out` is not touched. What
Go's `aead.Seal(dst[:0], nonce, plaintext, aad)` does into a slice with room.

1. Without additional data.
2. With `aad`, authenticated but not written.

`out` may be the plaintext itself, beginning at its first byte, with room for the tag after it: the plaintext is
then sealed in place.

## Parameters

| Parameter | Description |
|---|---|
| `out` | where the ciphertext and the tag go: at least `plaintext.size() + tag_size` bytes, the plaintext itself or apart from it |
| `nonce` | exactly `nonce_size` bytes of the class, never used twice under one key |
| `plaintext` | the bytes to encrypt, at most `max_plaintext_size` of the class |
| `aad` | the additional data |

## Return value

The bytes written, `plaintext.size() + tag_size`.

## Complexity

Linear in the sizes of `plaintext` and `aad`.

## Exceptions

- `invalid_argument` when `nonce` is not `nonce_size` bytes, or when `out` overlaps `plaintext` other than by
  beginning at the same byte.
- `length_error` when `plaintext` is longer than `max_plaintext_size`, or when `out` holds fewer bytes than the
  plaintext and the tag.
- `logic_error` when the object was moved from and holds no key.

Nothing is written then.

## Notes

An overlap shifted by a few bytes is refused, as Go panics on it: the cipher would read what it has just written.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // The test case 2 of the GCM specification, sealed in place: 16 zero bytes, room for the tag
    crypto::aes_gcm gcm(vector<byte>(16));
    vector<byte> nonce(12);
    array<byte, 32> buffer = {};
    size_t n = gcm.seal_to(buffer, nonce, buffer.as_slice(0, 16));
    println("{} {}", n, encoding::hex::encode(buffer));

    try {
        gcm.seal_to(buffer.as_slice(4, 28), nonce, buffer.as_slice(0, 12));
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
    try {
        gcm.seal_to(buffer.as_slice(0, 20), nonce, buffer.as_slice(0, 16));
    } catch (const length_error& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
32 0388dace60b6a392f328c2b971b2fe78ab6e47d42cec13bdf53a67b21257bddf
sgcl::crypto::aes_gcm::seal_to: the output overlaps the plaintext other than exactly
sgcl::crypto::aes_gcm::seal_to: the output holds fewer bytes than the plaintext and the tag
```

## See also

- [seal](seal.md): into a new vector
- [open_to](open_to.md): opens into the caller's buffer
- [sgcl::crypto::mixin::aead\<Derived\>](../aead.md)
