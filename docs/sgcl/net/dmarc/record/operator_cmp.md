[sgcl](../../../README.md) › [net](../../README.md) › [dmarc](../README.md) › [record](README.md)

# sgcl::net::dmarc::record::operator==

```cpp
friend bool operator==(const record&, const record&) noexcept = default;
```

Whether two records say the same: every member equal. Two texts that differ only in spacing, in the order of their
tags or in tags at their defaults parse to equal records.

## Parameters

None.

## Return value

`true` when every member is equal.

## Complexity

Linear in the records.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    net::dmarc::record a("v=DMARC1; p=reject; pct=100");
    net::dmarc::record b("v=DMARC1;p=reject");
    println("{}", a == b);
}
```

Output:

```text
true
```

## See also

- [record](README.md)
