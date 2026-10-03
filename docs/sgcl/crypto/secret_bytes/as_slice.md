[sgcl](../../README.md) › [crypto](../README.md) › [secret_bytes](../secret_bytes.md)

# sgcl::crypto::secret_bytes::as_slice, operator slice\<const byte\>, operator slice\<byte\>

```cpp
/*(1)*/ slice<const byte> as_slice() const noexcept;
/*(2)*/ slice<byte> as_slice() noexcept;
/*(3)*/ operator slice<const byte>() const noexcept;
/*(4)*/ operator slice<byte>() & noexcept;
```

Returns the bytes as a slice without an owner over the secret's own memory, its inline bytes or its block, valid
until the secret is resized, moved from or destroyed. Nothing is copied.

1. The bytes to read.
2. The bytes to write: what [random::fill](../random/fill.md), a key derivation's `derive_to` or an AEAD's `open_to`
   writes into.
3. Implicit, so that a secret goes wherever the module takes bytes, as a [secret\<N\>](../secret.md) does.
4. Implicit too, for a secret the program may change, so that it goes wherever the module writes bytes:
   [random::fill](../random/fill.md), an AEAD's `open_to`, rsa's `decrypt_oaep_to`. Not for a `const` secret, nor
   for a temporary one, whose bytes, once written, no one would read.

## Parameters

None.

## Return value

A slice of [size](size.md) bytes.

## Complexity

Constant.

## Exceptions

None.

## Notes

The slice is the secret's own place; what the program copies out of it (into a `vector`, a string) is a copy the
secret does not clear. A `secret_bytes` has no `operator[]` of its own: a byte is reached through the slice.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    crypto::secret_bytes keys(64);
    crypto::random::fill(keys);  // taken as a slice<byte>

    // two keys of one secret: the first half seals, the second is kept for later
    crypto::chacha20_poly1305 aead(keys.as_slice().first(32));
    auto nonce = encoding::hex::decode("070000004041424344454647");
    auto sealed = aead.seal(nonce, "attack at dawn");
    println("{} {}", aead.open(nonce, sealed).has_value(), keys.as_slice().subslice(32).size());
}
```

Output:

```text
true 32
```

## See also

- [size](size.md): the number of bytes
- [sgcl::crypto::secret_bytes](../secret_bytes.md)
