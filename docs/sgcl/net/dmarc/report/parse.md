[sgcl](../../../README.md) › [net](../../README.md) › [dmarc](../README.md) › [report](README.md)

# sgcl::net::dmarc::report::parse

```cpp
static expected<report, io::error> parse(const string& xml_text);
```

A report of its XML ([encoding::xml](../../../encoding/xml/README.md)): a `feedback` document with its
`report_metadata` (the period required), its `policy_published`, and its `record`s, each a row with a source address
and a count, its identifiers and its `auth_results`. What a receiver leaves out is left empty; values are taken
without the whitespace around them.

## Parameters

| Parameter | Description |
|---|---|
| `xml_text` | the report's XML, unzipped |

## Return value

The report; `net::errc::malformed_dmarc` for a text that is not XML, not a `feedback` document, without its
metadata, its policy or its period, or with a row without a source address or a count.

## Complexity

Linear in the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    auto r = net::dmarc::report::parse("<feedback><report_metadata/><policy_published/></feedback>");
    println("{}", r.error().message());
}
```

Output:

```text
dmarc report no date_range: malformed DMARC record
```

## See also

- [report](README.md)
