[sgcl](../../README.md) › [net](../README.md) › [tls](README.md) › [ech_key](ech_key/README.md)

# sgcl::net::tls::ech_key::options

```cpp
#include "sgcl/net/tls.h"

namespace sgcl::net::tls {
    class ech_key {
    public:
        struct options {
            optional<uint8_t> config_id;
            crypto::hpke::kem kem = crypto::hpke::kem::dhkem_x25519;
            vector<crypto::hpke::suite> suites;
            uint8_t max_name_length = 0;
            bool retry = true;
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`net::tls::ech_key::options` is what [generate](ech_key/generate.md) makes a new key of. Every field has a default:
`ech_key::generate("public.example", {.config_id = 2})`.

## Member objects

| Member | Description |
|---|---|
| `config_id` | the config's id, which a client's outer hello names; `nullopt`, the default: a random byte. Keys of one server should have different ids |
| `kem` | the KEM of the HPKE key ([crypto::hpke::kem](../../crypto/hpke-kem.md)): DHKEM(X25519) by default, what every client of ECH has |
| `suites` | the HPKE suites the config takes ([crypto::hpke::suite](../../crypto/hpke-suite.md)), the client choosing the first it has; empty, the default: HKDF-SHA256 with AES-128-GCM, AES-256-GCM and ChaCha20-Poly1305. An `export_only` suite is `std::invalid_argument` |
| `max_name_length` | the longest name the server serves behind the public name (RFC 9849 §6.1.3): a client pads the name of its inner hello to it, so that names of other lengths look alike; 0, the default: none named, the hellos padded to a multiple of 32 alone |
| `retry` | whether the config is sent in the `retry_configs` of a rejected hello; `true` by default |

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    auto key = net::tls::ech_key::generate("public.example",
        {.config_id = 7, .suites = {crypto::hpke::suite{.aead = crypto::hpke::aead::chacha20_poly1305}}});
    println("{} {}", key.config_id(), key.public_name());
}
```

Output:

```text
7 public.example
```

## See also

- [ech_key](ech_key/README.md)
- [net::tls](README.md)
