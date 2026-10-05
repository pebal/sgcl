[sgcl](../../../README.md) › [net](../../README.md) › [acme](../README.md) › [manager](README.md)

# sgcl::net::acme::manager::manager

```cpp
explicit manager(const vector<string>& names);             // (1)
manager(const vector<string>& names, const options& o);    // (2)
explicit manager(const options& o);                        // (3)
manager(const manager& other) = default;                   // (4)
```

1. A manager of the names, from Let's Encrypt, in memory alone, without its terms agreed to (which Let's Encrypt
   requires: (2) says `accept_terms`). A name is an exact name, served and obtained as it is, or a wildcard
   `*.example.com`, served to the names one label under it and obtained by dns-01.
2. The same with the [options](../manager-options.md) `o`.
3. A manager of the names `o.host_policy` allows.
4. The same manager: a copy of the handle.

Nothing is sent until the first handshake or call that needs a certificate.

## Parameters

| Parameter | Description |
|---|---|
| `names` | the names the server answers to |
| `o` | where certificates come from and how |
| `other` | the manager to share |

## Complexity

Linear in the number of names.

## Exceptions

None but running out of memory.

## Example

A server of two names from Let's Encrypt (the names must resolve to the machine, ports 80 and 443 open to the CA):

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    // Let's Encrypt, the account's key and the certificates kept in a directory
    net::acme::manager certificates({"example.com", "www.example.com"},
                                    {.cache = "/var/lib/myapp/certs",
                                     .contact = {"mailto:admin@example.com"},
                                     .accept_terms = true});
    net::http::server http;   // port 80: http-01, and everything else to https
    http.route("/", certificates.http_handler());
    auto plain = async::spawn(http.async_serve(":80"));
    auto served = net::http::serve_tls(":443", certificates.tls_config(),
                                       [](net::http::request req, net::http::response_writer w) {
                                           w.write("hello from " + req.url().host());
                                       });
    println("{}", served.error().message());
}
```

Against a test server of the program's own:

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::test_server ca({.skip_validation = true});
    net::acme::manager certificates({"example.com"},
                                    {.directory_url = ca.directory_url(), .accept_terms = true});
    net::acme::manager same = certificates;
    net::tls::identity id = same.certificate("example.com");
    println("{}, {} order", id.certificates()[0].dns_names()[0], ca.orders());
    certificates.close();
}
```

Output:

```text
example.com, 1 order
```

## See also

- [options](../manager-options.md)
- [sgcl::net::acme::manager](README.md)
