[sgcl](../../README.md) › [crypto](../README.md) › [hpke](../hpke.md) › [sender](README.md)

# sgcl::crypto::hpke::sender::export_secret

```cpp
secret_bytes export_secret(const slice<const byte>& context, size_t length) const;
```

A secret of the context for a label (RFC 9180 §5.3): the KDF's expand of the exporter secret over `context`. The
recipient's context exports the same secret for the same label: a key for another protocol, bound to this exchange.

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
    auto a = s->export_secret("session key", 32);
    auto b = r->export_secret("session key", 32);
    println("{}", a == b);
}
```

Output:

```text
true
```

## See also

- [recipient::export_secret](../hpke-recipient/export_secret.md)
- [sgcl::crypto::hpke::sender](README.md)
