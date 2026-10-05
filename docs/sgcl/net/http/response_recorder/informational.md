[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response_recorder](README.md)

# sgcl::net::http::response_recorder::informational

```cpp
vector<pair<int, http::headers>> informational() const noexcept;
```

Returns the informational responses the handler sent
([send_informational](../response_writer/send_informational.md)), in their order: each status and its fields.

## Parameters

None.

## Return value

The responses, a status and its fields each.

## Complexity

Linear in their number.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::response_recorder rec;
    auto w = rec.writer();
    net::http::headers hints;
    hints.add("Link", "</app.js>; rel=preload; as=script");
    w.send_informational(net::http::status::early_hints, hints);
    w.write("page");
    for (auto& [code, fields] : rec.informational()) {
        println("{} {}", code, fields.get("Link"));
    }
}
```

Output:

```text
103 </app.js>; rel=preload; as=script
```

## See also

- [response_writer::send_informational](../response_writer/send_informational.md)
- [sgcl::net::http::response_recorder](README.md)
