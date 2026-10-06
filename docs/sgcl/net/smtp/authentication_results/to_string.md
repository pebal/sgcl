[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [authentication_results](README.md)

# sgcl::net::smtp::authentication_results::to_string

```cpp
string to_string() const;
```

The value on one line: the authserv-id, then each result as `method=result`, its `reason=` and its properties, the
results separated by `; `; `none` when there are none. A value that is not a token is quoted. A server folds the
line where it puts the field into a message; [parse](parse.md) reads it back to the same value.

## Parameters

None.

## Return value

The value: `"mx.example.org; spf=pass smtp.mailfrom=example.com; dkim=pass header.d=example.com"`.

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
    net::smtp::authentication_results ar;
    ar.authserv_id = "mx.example.org";
    ar.results.push_back({"dkim", "fail", "bad signature", {{"header.d", "example.com"}}});
    println("{}", ar.to_string());
}
```

Output:

```text
mx.example.org; dkim=fail reason="bad signature" header.d=example.com
```

## See also

- [parse](parse.md)
- [authentication_results](README.md)
