[sgcl](../../README.md) › [net](../README.md) › [http](README.md) › [test_server](test_server/README.md) › options

# sgcl::net::http::test_server::options

```cpp
#include "sgcl/net/http/test.h"   // or "sgcl/net/http.h"

namespace sgcl::net::http {
    class test_server {
    public:
        struct options {
            bool tls = false;
            bool http2 = false;
            bool keep_requests = false;
        };
    };
}
```

`sgcl::net::http::test_server::options` is how a [test_server](test_server/README.md) serves: a plain struct, its
fields set by name (`{.tls = true}`); the constructors without it take its defaults, plain HTTP/1.1.

## Member objects

| Member | Description |
|---|---|
| `tls` | https: a test CA and a leaf it signed (`localhost`, `127.0.0.1`, `::1`) made at the start, the [client](test_server/client.md) trusting the CA; off by default |
| `http2` | HTTP/2: by ALPN (`h2`) over TLS, by prior knowledge (h2c) without, and the client speaking it; HTTP/1.1 is served beside it either way; off by default |
| `keep_requests` | every request kept, as it reached its route, for [requests](test_server/requests.md); off by default |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    auto proto = [](net::http::request r, net::http::response_writer w) {
        w.write(r.proto());
    };
    net::http::test_server plain(proto);
    net::http::test_server h2c(proto, {.http2 = true});
    net::http::test_server h2(proto, {.tls = true, .http2 = true});
    for (auto* ts : {&plain, &h2c, &h2}) {
        string answer = ts->client().get(ts->url())->text().value();
        println("{} {}", ts->url().view().substr(0, 5), answer);
    }
}
```

Output:

```text
http: HTTP/1.1
http: HTTP/2.0
https HTTP/2.0
```

## See also

- [test_server](test_server/README.md)
