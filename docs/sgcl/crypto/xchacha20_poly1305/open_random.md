[sgcl](../../README.md) › [crypto](../README.md) › [xchacha20_poly1305](../xchacha20_poly1305.md)

# sgcl::crypto::xchacha20_poly1305::open_random

```cpp
/*(1)*/ [[nodiscard]] expected<vector<byte>, error>
        open_random(const slice<const byte>& sealed) const;
/*(2)*/ [[nodiscard]] expected<vector<byte>, error>
        open_random(const slice<const byte>& sealed, const slice<const byte>& aad) const;
```

Opens what [seal_random](seal_random.md) made: the nonce read from the first 24 bytes of `sealed`, the tag from the
last 16, checked before the first byte is decrypted, as [open](../mixin/aead/open.md) does.

1. Without additional data.
2. With `aad`, the additional data the sender gave to `seal_random`.

## Parameters

| Parameter | Description |
|---|---|
| `sealed` | the nonce, the ciphertext and the tag, as `seal_random` gives them |
| `aad` | the additional data the data was sealed with |

## Return value

The plaintext, `sealed.size() - nonce_size - tag_size` bytes, or an [error](../error.md) of
[errc::authentication](../errc.md) when the tag does not match and when `sealed` is shorter than a nonce and a
tag.

## Complexity

Linear in the sizes of `sealed` and `aad`.

## Exceptions

`logic_error` when the object was moved from and holds no key.

## Notes

The plaintext is the user's data, a managed `vector<byte>` as [open](../mixin/aead/open.md) gives it. A plaintext
that must not stay in memory is opened with [open_to](../mixin/aead/open_to.md), the nonce the first 24 bytes and
the sealed data the rest.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::xchacha20_poly1305 aead(vector<byte>(32));
    string text = "attack at dawn";
    auto sealed = aead.seal_random(text);
    println("{}", string(aead.open_random(sealed)));

    sealed[0] ^= byte(1);  // the nonce changed on the way
    auto forged = aead.open_random(sealed);
    println("{}", forged.error().message());
    println("{}", aead.open_random(sealed.as_slice(0, 39)).has_value());
}
```

Output:

```text
attack at dawn
message authentication failed
false
```

## See also

- [seal_random](seal_random.md): seals under a random nonce
- [open](../mixin/aead/open.md): under a nonce the program gives
- [sgcl::crypto::xchacha20_poly1305](../xchacha20_poly1305.md)
