[sgcl](../../../README.md) › [net](../../README.md) › [tls](../README.md) › [ech_key](README.md)

# sgcl::net::tls::ech_key::config

```cpp
vector<byte> config() const;
```

The ECHConfig (RFC 9849 §4), its version and length first: what [from_bytes](from_bytes.md) takes back, and what [ech_config_list](../ech_config_list.md) lists.

## Parameters

None.

## Return value

The bytes.

## Complexity

Linear in the config's length.

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
    auto c = key.config();
    println("{:02x}{:02x}", int(c[0]), int(c[1]));
}
```

Output:

```text
fe0d
```

## See also

- [ech_config_list](../ech_config_list.md)
- [sgcl::net::tls::ech_key](README.md)
