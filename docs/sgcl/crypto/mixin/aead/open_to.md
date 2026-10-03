[sgcl](../../../README.md) › [crypto](../../README.md) › [aead](README.md)

# sgcl::crypto::mixin::aead\<Derived\>::open_to

```cpp
[[nodiscard]] expected<size_t, error> open_to(const slice<byte>& out,                    // (1)
                                              const slice<const byte>& nonce,
                                              const slice<const byte>& sealed) const;
[[nodiscard]] expected<size_t, error> open_to(const slice<byte>& out,                    // (2)
                                              const slice<const byte>& nonce,
                                              const slice<const byte>& sealed,
                                              const slice<const byte>& aad) const;
```

Opens as [open](open.md) does, into a buffer the caller gives, with nothing allocated: the plaintext is written to
the first `sealed.size() - tag_size` bytes of `out`, and the rest of `out` is not touched.

1. Without additional data.
2. With `aad`, the additional data the sender gave to [seal_to](seal_to.md).

`out` may be the sealed data itself, beginning at its first byte: the data is then opened in place. When the tag
does not match, the bytes the plaintext would have taken are set to zero and nothing else of `out` is touched: a
program that ignores the error reads zeros, neither the attacker's text nor the ciphertext, and opened in place the
ciphertext is zeroed too.

## Parameters

| Parameter | Description |
|---|---|
| `out` | where the plaintext goes: at least `sealed.size() - tag_size` bytes, the sealed data itself or apart from it |
| `nonce` | the nonce the data was sealed with, exactly `nonce_size` bytes of the class |
| `sealed` | the ciphertext followed by the tag |
| `aad` | the additional data the data was sealed with |

## Return value

The length of the plaintext, `sealed.size() - tag_size`, or an [error](../../error/README.md) of
[errc::authentication](../../errc.md) when the tag does not match, when `sealed` is shorter than a tag and when it
is longer than `max_plaintext_size + tag_size`.

## Complexity

Linear in the sizes of `sealed` and `aad`.

## Exceptions

- `invalid_argument` when `nonce` is not `nonce_size` bytes, or when `out` overlaps `sealed` other than by beginning
  at the same byte.
- `length_error` when `out` holds fewer bytes than the sealed data less the tag.
- `logic_error` when the object was moved from and holds no key.

Nothing is written then. Sealed data shorter than a tag, or longer than a seal can make, is the error and not an
exception, whatever the size of `out`.

## Notes

The way to open a plaintext that must not stay in memory, a key unwrapped or a password: into a
[secret_bytes](../../secret_bytes/README.md) of its length, which zeroes its bytes when it goes, or into a buffer the
program clears with [secure_zero](../../secure_zero.md) when done.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    // The test case 2 of the GCM specification: what was sealed is a key of 16 zero bytes
    crypto::aes_gcm gcm(vector<byte>(16));
    vector<byte> nonce(12);
    vector<byte> sealed = encoding::hex::decode(
        "0388dace60b6a392f328c2b971b2fe78ab6e47d42cec13bdf53a67b21257bddf");

    crypto::secret_bytes unwrapped(16);
    auto n = gcm.open_to(unwrapped, nonce, sealed);
    println("{} {}", *n, encoding::hex::encode(unwrapped));

    array<byte, 32> buffer;
    for (int i : range(32)) {
        buffer[i] = sealed[i];
    }
    buffer[0] ^= byte(1);  // a forgery, opened in place
    auto forged = gcm.open_to(buffer, nonce, buffer);
    println("{}", forged.error().message());
    println("{}", encoding::hex::encode(buffer));
}
```

Output:

```text
16 00000000000000000000000000000000
message authentication failed
00000000000000000000000000000000ab6e47d42cec13bdf53a67b21257bddf
```

## See also

- [open](open.md): into a new vector
- [seal_to](seal_to.md): seals into the caller's buffer
- [secret_bytes](../../secret_bytes/README.md): a buffer for a plaintext that is a secret
- [sgcl::crypto::mixin::aead\<Derived\>](README.md)
