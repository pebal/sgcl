[sgcl](../README.md) › [crypto](README.md) › [hpke](hpke.md)

# sgcl::crypto::hpke::open

```cpp
expected<vector<byte>, error> open(const private_key& key, const slice<const byte>& sealed, const suite& s = {},
                                   const slice<const byte>& info = {}, const slice<const byte>& aad = {});
```

The message of a single-shot [seal](hpke-seal.md) (RFC 9180 §6.1, Go's `hpke.Open`): enc read from the front of
`sealed`, a [recipient](hpke-recipient/README.md) made of it and `key`, the rest opened. `[[nodiscard]]`.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the recipient's private key |
| `sealed` | what seal gave |
| `s` | the suite, the sender's |
| `info` | the sender's info; none by default |
| `aad` | the sender's aad; none by default |

## Return value

The message, or an error: `errc::malformed` for less than enc, what [recipient::setup](hpke-recipient/setup.md)
refuses, `errc::authentication` for a message that does not open.

## Complexity

Linear in the length of the message, and a Diffie-Hellman operation.

## Exceptions

`std::logic_error` for an `export_only` suite.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto key = crypto::hpke::private_key::generate(crypto::hpke::kem::dhkem_x25519);
    auto sealed = crypto::hpke::seal(key.public_key(), "hello", {}, "v1");
    println("{}", string(crypto::hpke::open(key, sealed.value(), {}, "v1").value()));
    println("{}", crypto::hpke::open(key, sealed.value(), {}, "v2").error().message());
}
```

Output:

```text
hello
message authentication failed
```

## See also

- [seal](hpke-seal.md)
- [sgcl::crypto::hpke](hpke.md)
