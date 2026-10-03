[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [response](../response.md)

# sgcl::net::http::response::status

```cpp
int status() const noexcept;
```

Returns the status code of the response, Go's `resp.StatusCode`: the final one, after the 1xx responses that came
before it were skipped and the redirects followed. A program compares it with the constants of
[status](../status.md); [reason](../reason.md) gives its phrase.

## Parameters

None.

## Return value

The status code.

## Complexity

Constant.

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
    srv.route("POST /notes", [](net::http::request, net::http::response_writer w) {
        w.set_status(net::http::status::created);
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());

    net::http::client web;
    for (string path : {"/notes", "/other"}) {
        net::http::response res = web.post(base + path, "text/plain", "buy milk");
        println("{} {}", res.status(), net::http::reason(res.status()));
        res.close();
    }
    srv.close();
}
```

Output:

```text
201 Created
404 Not Found
```

## See also

- [ok](ok.md): whether it is 2xx
- [status](../status.md): the codes as constants
- [sgcl::net::http::response](../response.md)
