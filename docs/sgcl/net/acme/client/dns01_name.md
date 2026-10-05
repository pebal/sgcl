[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [client](README.md)

# sgcl::net::acme::client::dns01_name

```cpp
static string dns01_name(const string& domain);
```

The name of dns-01's TXT record for a domain (RFC 8555 §8.4): `_acme-challenge.` and the domain, a wildcard's without
its `*.`.

## Parameters

| Parameter | Description |
|---|---|
| `domain` | the authorization's identifier, or the name ordered |

## Return value

The record's name.

## Complexity

Linear in the length of the domain.

## Exceptions

None but running out of memory.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    println("{}", net::acme::client::dns01_name("*.example.com"));
}
```

Output:

```text
_acme-challenge.example.com
```

## See also

- [dns01_value](dns01_value.md)
- [sgcl::net::acme::client](README.md)
