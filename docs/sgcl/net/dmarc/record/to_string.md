[sgcl](../../../README.md) › [net](../../README.md) › [dmarc](../README.md) › [record](README.md)

# sgcl::net::dmarc::record::to_string

```cpp
string to_string() const;
```

The record's text, to publish at `_dmarc.<domain>`: `v=DMARC1` and `p=` first, then each tag whose value is not
its default, in the order of the members. [parse](parse.md) reads it back to the same record.

## Parameters

None.

## Return value

The text: `"v=DMARC1; p=reject; sp=none; adkim=s; pct=50; rua=mailto:d@example.com"`.

## Complexity

Linear in the record.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    net::dmarc::record r("v=DMARC1;p=reject;sp=none;aspf=s;ri=86400;rf=afrf");
    println("{}", r.to_string());
}
```

Output:

```text
v=DMARC1; p=reject; sp=none; aspf=s
```

## See also

- [parse](parse.md)
- [record](README.md)
