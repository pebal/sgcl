[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [sender_verdict](README.md)

# sgcl::net::smtp::sender_verdict::results

```cpp
authentication_results results(const string& authserv_id) const;
```

The results as an Authentication-Results field of the receiver `authserv_id` (RFC 8601): each check made, in the
order SPF, DKIM, DMARC. SPF with `smtp.mailfrom` (the domain checked); a DKIM result per signature with `header.d`,
`header.i`, `header.s`, `header.a` and `header.b` (the signature's first eight characters, which tell two
signatures of one domain apart), `dkim=none` for a message without one; DMARC with `policy.dmarc` (the disposition)
and `header.from`. A result that is not a pass or none carries its reason.

## Parameters

| Parameter | Description |
|---|---|
| `authserv_id` | the receiver's name, the field's authserv-id |

## Return value

The [authentication_results](../authentication_results/README.md); `"<id>; none"` written when no check was made.

## Complexity

Linear in the results.

## Exceptions

None.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    net::spf::result spf;
    spf.status = net::spf::status::pass;
    spf.domain = "example.com";
    net::smtp::sender_verdict a;
    a.spf = spf;
    println("Authentication-Results: {}", a.results("mx.example.org").to_string());
}
```

Output:

```text
Authentication-Results: mx.example.org; spf=pass smtp.mailfrom=example.com
```

## See also

- [authentication_results](../authentication_results/README.md)
- [sender_verdict](README.md)
