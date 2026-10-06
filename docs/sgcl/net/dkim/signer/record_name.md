[sgcl](../../../README.md) › [net](../../README.md) › [dkim](../README.md) › [signer](README.md)

# sgcl::net::dkim::signer::record_name

```cpp
string record_name() const;
```

Where the key's record is published, and where a receiver asks for it: `<selector>._domainkey.<domain>`.

## Parameters

None.

## Return value

The name, without a dot at its end.

## Complexity

Linear in the name.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    auto s = net::dkim::signer::generate("example.com", "s2026", net::dkim::algorithm::ed25519_sha256);
    println("{}", s.record_name());
}
```

Output:

```text
s2026._domainkey.example.com
```

## See also

- [record](record.md)
- [signer](README.md)
