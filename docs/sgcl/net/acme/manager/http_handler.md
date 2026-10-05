[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [manager](README.md)

# sgcl::net::acme::manager::http_handler

```cpp
function<void(http::request, http::response_writer)> http_handler() const;
```

A handler for port 80, Go's `HTTPHandler(nil)`: a GET of `/.well-known/acme-challenge/` and a token of an http-01
challenge under way is answered with its key authorization (404 for any other token); any other GET or HEAD is
redirected (302) to the same path and query on https and the default port; another method is 400. Routed at `"/"` of
the port-80 [server](../../http/server/README.md): `http.route("/", certificates.http_handler())`. With `"http-01"`
first in `options::challenges`, the manager obtains its certificates through it.

## Parameters

None.

## Return value

The handler, holding the manager.

## Complexity

Constant.

## Exceptions

None but running out of memory.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    // the CA validates http-01 at the port the handler serves
    net::listener port80 = net::tcp::listen("127.0.0.1:0");
    net::acme::test_server ca({.http_port = port80.local_endpoint().port()});
    net::acme::manager certificates({"example.com"},
                                    {.directory_url = ca.directory_url(), .accept_terms = true,
                                     .challenges = {"http-01"}});
    net::http::server http;
    http.route("/", certificates.http_handler());
    auto serving = async::spawn(http.async_serve(port80));
    net::tls::identity id = certificates.certificate("example.com");
    println("{}", id.certificates()[0].dns_names()[0]);
    certificates.close();
    http.close();
    serving.wait();
}
```

Output:

```text
example.com
```

## See also

- [client::http01_path](../client/http01_path.md)
- [sgcl::net::acme::manager](README.md)
