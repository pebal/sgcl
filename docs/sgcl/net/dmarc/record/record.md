[sgcl](../../../README.md) › [net](../../README.md) › [dmarc](../README.md) › [record](README.md)

# sgcl::net::dmarc::record::record

```cpp
record() = default;                     // (1)
explicit record(const string& text);    // (2)
```

1. A record of the defaults: `p=none`, relaxed alignment, `pct=100`, no reports.
2. The record a literal spells, as [parse](parse.md) reads it; what parse refuses is thrown (DESIGN 234: text the
   program wrote itself is constructed, input is parsed).

## Parameters

| Parameter | Description |
|---|---|
| `text` | the record's text |

## Complexity

- (1) Constant.
- (2) Linear in the text.

## Exceptions

- (1) None.
- (2) `bad_expected_access<io::error>` with [parse](parse.md)'s error.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    net::dmarc::record r("v=DMARC1; p=reject; adkim=s");
    println("{} {}", net::dmarc::to_string(r.policy), r.strict_dkim);
}
```

Output:

```text
reject true
```

## See also

- [parse](parse.md)
- [record](README.md)
