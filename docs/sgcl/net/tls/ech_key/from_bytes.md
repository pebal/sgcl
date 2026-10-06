[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [ech_key](README.md)

# sgcl::net::tls::ech_key::from_bytes

```cpp
static expected<ech_key, io::error> from_bytes(const slice<const byte>& config, const slice<const byte>& private_key,
                                               bool retry = true) noexcept;
```

A key of an ECHConfig and its HPKE private key, as Go's `EncryptedClientHelloKey` has them and as [config](config.md) and [private_key](private_key.md) give them: what a server keeps between its runs. The private key's bytes are read where they lie (a [secret_bytes](../../../crypto/secret_bytes/README.md) of [read_secret](../../../crypto/read_secret.md)).

## Parameters

| Parameter | Description |
|---|---|
| `config` | one ECHConfig of version 0xfe0d |
| `private_key` | the HPKE private key (SerializePrivateKey) |
| `retry` | whether the config is sent in retry_configs; `true` by default |

## Return value

The key, or an `io::error` of op `ech key`: `crypto::errc::malformed` for a config that does not read or is of a KEM the module does not have, `crypto::errc::invalid_key` for a private key that does not read or is not the config's.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    auto key = net::tls::ech_key::generate("public.example");
    auto again = net::tls::ech_key::from_bytes(key.config(), key.private_key());
    println("{}", again->config_id() == key.config_id());
}
```

Output:

```text
true
```

## See also

- [generate](generate.md)
- [sgcl::net::tls::ech_key](README.md)
