[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [form](README.md)

# sgcl::net::http::form::content_type

```cpp
string content_type() const noexcept;
```

The `Content-Type` of a request whose body is the form, `multipart/form-data; boundary=` and its
[boundary](boundary.md), Go's `Writer.FormDataContentType`. [request::set_body](../request/set_body.md) of the form
sets it on the request.

## Parameters

None.

## Return value

The field's value.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::form f{{"a", "1"}};
    println("{}", f.content_type() == "multipart/form-data; boundary=" + f.boundary());
    net::http::request r("POST", "http://example.com/upload");
    r.set_body(f);
    println("{}", r.header("Content-Type") == f.content_type());
}
```

Output:

```text
true
true
```

## See also

- [boundary](boundary.md)
- [form](README.md)
