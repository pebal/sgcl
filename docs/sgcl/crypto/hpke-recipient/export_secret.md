[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md) › [recipient](README.md)

# sgcl::crypto::hpke::recipient::export_secret

```cpp
secret_bytes export_secret(const slice<const byte>& context, size_t length) const;
```

A secret of the context for a label (RFC 9180 §5.3): the one the sender's context exports for the same label.

## Parameters

| Parameter | Description |
|---|---|
| `context` | the label, bytes or text |
| `length` | the bytes asked, at most 255 times the KDF's hash's length |

## Return value

The secret, in a secret_bytes.

## Complexity

Linear in `length`.

## Exceptions

`std::invalid_argument` past 255 times the hash's length; `std::logic_error` for a context moved from.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_x25519);
    auto s = crypto::hpke::sender::setup(key.public_key(), {}, "app");
    auto r = crypto::hpke::recipient::setup(s->enc(), key, {}, "app");
    println("{}", r->export_secret("k", 16) == s->export_secret("k", 16));
}
```

Output:

```text
true
```

## See also

- [sender::export_secret](../hpke-sender/export_secret.md)
- [sgcl::crypto::hpke::recipient](README.md)
