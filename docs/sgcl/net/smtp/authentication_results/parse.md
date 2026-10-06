[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [authentication_results](README.md)

# sgcl::net::smtp::authentication_results::parse

```cpp
static expected<authentication_results, io::error> parse(const string& value);
```

The value of an Authentication-Results field (what follows its colon), read by RFC 8601 §2.2's grammar: the
authserv-id (a token or a quoted string), a version that must be 1 when it is there, then `none` or a result per
method — `method[/version]=result`, `reason=`, and properties `ptype.property=value` — separated by `;`. Comments
and folding are passed over; methods, results and the names of properties are kept in lower case, values as they
are.

## Parameters

| Parameter | Description |
|---|---|
| `value` | the field's value |

## Return value

The value read; `net::errc::malformed_message` for one that breaks the grammar.

## Complexity

Linear in the value.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    auto ar = net::smtp::authentication_results::parse(
        "mx.example.org (Postfix); dkim=fail (bad signature) reason=\"body changed\" header.d=example.com");
    auto& r = ar->results[0];
    println("{} {} {}", r.result, r.reason, r.properties[0].second);
    println("{}", net::smtp::authentication_results::parse("mx; spf").error().message());
}
```

Output:

```text
fail body changed example.com
authentication results malformed Authentication-Results: malformed mail message
```

## See also

- [to_string](to_string.md)
- [authentication_results](README.md)
