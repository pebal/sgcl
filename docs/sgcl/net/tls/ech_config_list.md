[sgcl](../../README.md) › [net](../README.md) › [tls](README.md)

# sgcl::net::tls::ech_config_list

```cpp
vector<byte> ech_config_list(const vector<ech_key>& keys);
```

The ECHConfigList of the keys' configs (RFC 9849 §4), in their order: what a server publishes in the `ech` of its
DNS HTTPS record (RFC 9848), and what a client's [config](config.md)'s `ech_config_list` takes.

## Parameters

| Parameter | Description |
|---|---|
| `keys` | the server's keys ([ech_key](ech_key/README.md)) |

## Return value

The list; empty for no keys.

## Complexity

Linear in the configs' lengths.

## Exceptions

None.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    auto current = net::tls::ech_key::generate("public.example", {.config_id = 2});
    auto next = net::tls::ech_key::generate("public.example", {.config_id = 3});
    println("{}", net::tls::ech_config_list({current, next}).size());
}
```

Output:

```text
148
```

## See also

- [ech_key](ech_key/README.md)
- [config](config.md): `ech_config_list`, `ech_from_dns`
- [dns::lookup_https](../dns/lookup_https.md): the list read back from the HTTPS record
- [net::tls](README.md)
