[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](../request.md)

# sgcl::net::http::request::set_header

```cpp
request& set_header(const string& name, const string& value) noexcept;
```

Sets the field `name` to `value`, Go's `r.Header.Set`: `headers().set(name, value)` ([set](../headers/set.md)), the
value in the place of the first field of the name and the others dropped. The name and the value are kept as given
and checked by the send: a name that is not a token or a value with CR, LF, NUL or another control makes it
`std::errc::invalid_argument`, before a byte is sent. A `Host` set here is sent in place of the URL's.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the field |
| `value` | the value |

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
    srv.route("GET /", [](net::http::request req, net::http::response_writer w) {
        w.write(req.header("Accept") + "\n");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    net::http::request req("GET", base + "/");
    req.set_header("Accept", "text/html").set_header("accept", "text/plain");
    print("{}", web.send(req)->text().value());

    req.set_header("X-Name", "line one\r\nInjected: yes");
    println("{}", web.send(req).error().message());
    srv.close();
}
```

Output:

```text
text/plain
send invalid header value: X-Name: Invalid argument
```

## See also

- [add_header](add_header.md): a field appended
- [header](header.md): a field read
- [sgcl::net::http::request](../request.md)
