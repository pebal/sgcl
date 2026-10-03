[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response_writer](README.md)

# sgcl::net::http::response_writer::set_header

```cpp
response_writer& set_header(const string& name, const string& value) noexcept;
```

Sets the field `name` to `value`, Go's `w.Header().Set`: the [set](../headers/set.md) of the response's
[headers](headers.md). The name is compared without regard to case; the value takes the place of the first field of
the name, which keeps its place and its name as first written, and the others of the name are dropped; a field of a
new name goes at the end. The field is checked when the head goes, not here: a name that is not a token or a value
with CR, LF, NUL or another control fails the response ([response_writer](README.md#rules)). After the
head has gone, the field goes nowhere.

The framing fields are the server's: a Transfer-Encoding set here is not sent, a Content-Length is replaced by the
true length of a body sent whole. `Connection: close` ends the connection after the response.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the field |
| `value` | its value |

## Return value

`*this`.

## Complexity

Linear in the number of fields.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::http::server srv;
    srv.route("GET /", [](net::http::request, net::http::response_writer w) {
        w.set_header("Cache-Control", "no-cache");
        w.set_header("cache-control", "max-age=60");
        w.set_header("Content-Length", "1000");
        w.write("short\n");
    });
    net::listener incoming = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(incoming));

    net::http::client web;
    net::http::response res =
        web.get("http://127.0.0.1:" + to_string(incoming.local_endpoint().port()) + "/");
    string text = res.text();
    println("{} | {}", res.header("Cache-Control"), res.header("Content-Length"));
    srv.shutdown();
}
```

Output:

```text
max-age=60 | 6
```

## See also

- [add_header](add_header.md): a field beside the others of its name
- [headers](headers.md): all the fields
- [sgcl::net::http::response_writer](README.md)
