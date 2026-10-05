[sgcl](../../README.md) › [net](../README.md) › [http](README.md)

# sgcl::net::http::test_request

```cpp
#include "sgcl/net/http/test.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    request test_request(const string& method, const string& target, const string& body = string());
}
```

Makes a [request](request/README.md) as a server hands one to its handler, Go's `httptest.NewRequest`, for a handler
called with a [response_recorder](response_recorder/README.md)'s writer or run through a server's routes by
[serve](response_recorder/serve.md). The target is a path with its query, `"/users?id=7"`, the host then
`example.com`, or a URL, whose host and port it takes (a path without its slash gets one). The request is HTTP/1.1,
from `192.0.2.1:1234` (TEST-NET-1, as Go's), with a `Host` field; its body is read through
[text](request/text.md), [bytes](request/bytes.md) and [body](request/body.md) as a received one is, once, with
its Content-Length (and a `Content-Length` field) when it is not empty. Its [stop](request/stop.md) is a token
never stopped; [tls](request/tls.md) is `nullopt`, whatever the scheme of the target. Fields are added to it with
[set_header](request/set_header.md).

## Parameters

| Parameter | Description |
|---|---|
| `method` | the method, as given (`"GET"`, `"POST"`) |
| `target` | a path with its query, or an `http://` or `https://` URL |
| `body` | the body; none by default |

## Return value

The request.

## Complexity

Linear in the lengths of the target and the body.

## Exceptions

`invalid_argument` for an empty target or one that does not parse.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    auto r = net::http::test_request("POST", "/orders?express=1", "two pens");
    r.set_header("Content-Type", "text/plain");
    println("{} {} {} | {} {}", r.method(), r.url().to_string(), r.proto(), r.header("Host"),
            r.content_length().value_or(0));
    println("{} | {}", r.remote_endpoint().to_string(), r.text().value());

    auto api = net::http::test_request("GET", "https://api.example.com:8443/v1/items");
    println("{} {}", api.header("Host"), api.url().path());
}
```

Output:

```text
POST http://example.com/orders?express=1 HTTP/1.1 | example.com 8
192.0.2.1:1234 | two pens
api.example.com:8443 /v1/items
```

## See also

- [response_recorder](response_recorder/README.md): what the handler writes to
- [request](request/README.md): the type it makes
- [test_server](test_server/README.md): a handler served on the loopback
