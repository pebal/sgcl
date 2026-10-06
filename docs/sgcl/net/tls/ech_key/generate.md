[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [ech_key](README.md)

# sgcl::net::tls::ech_key::generate

```cpp
static ech_key generate(const string& public_name);
static ech_key generate(const string& public_name, const options& o);
```

A new key of the public name: a fresh HPKE key of the KEM and its ECHConfig (version 0xfe0d) with the id, the suites and the longest name of `o` ([options](../ech_key-options.md)).

## Parameters

| Parameter | Description |
|---|---|
| `public_name` | the name the outer hellos name, a DNS name the server has a certificate of |
| `o` | the id, the KEM, the suites, the longest name, retry |

## Return value

The key.

## Complexity

Constant: a key generated.

## Exceptions

`std::invalid_argument` for a public name that is no DNS name (an address, a label of other characters than letters, digits and `-`, an empty label) and for an `export_only` suite.

## Example

```cpp
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/tls.h"

using namespace sgcl;

int main() {
    auto key = net::tls::ech_key::generate("public.example", {.config_id = 1});
    println("{} {} bytes", key.public_name(), key.config().size());
}
```

Output:

```text
public.example 73 bytes
```

## See also

- [from_bytes](from_bytes.md): a key the program kept
- [sgcl::net::tls::ech_key](README.md)
