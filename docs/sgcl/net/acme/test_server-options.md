[sgcl](../../README.md) › [net](../README.md) › [acme](README.md)

# sgcl::net::acme::test_server::options

```cpp
#include "sgcl/net/acme/test_server.h"   // or "sgcl/net/acme.h"

namespace sgcl::net::acme {
    class test_server {
    public:
        struct options {
            string address = string("127.0.0.1:0");
            bool tls = false;
            string validation_host = string("127.0.0.1");
            uint16_t http_port = 80;
            uint16_t tls_port = 443;
            function<async::task<expected<vector<string>, io::error>>(const string&)>
                lookup_txt;
            bool skip_validation = false;
            bool reuse_authorizations = false;
            duration certificate_lifetime = 90 * 24 * hour;
            duration processing_time = duration::zero();
            string terms_of_service;
            bool require_external_account = false;
            vector<external_account> external_accounts;
            int alternate_chains = 0;
            bool renewal_info = true;
            ordered_map<string, string> profiles;
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

What a [test_server](test_server/README.md) is and how it validates, given to its [constructor](test_server/test_server.md).
A designated initializer names what differs: `net::acme::test_server ca({.skip_validation = true})`.

## Member objects

| Object | Description |
|---|---|
| `address` | where it listens; a port of the system's choice on the loopback by default |
| `tls` | the API over https, with a certificate of its CA for `localhost`, `127.0.0.1` and `::1`; `false` by default |
| `validation_host` | the address every name is validated at; the loopback by default |
| `http_port` | http-01: the port of `http://name:port/.well-known/acme-challenge/`; 80 by default |
| `tls_port` | tls-alpn-01: the port of the TLS server, ALPN `acme-tls/1`; 443 by default |
| `lookup_txt` | dns-01: the TXT records of a name; empty, the default, finds none |
| `skip_validation` | every challenge valid as it is answered, nothing validated; `false` by default |
| `reuse_authorizations` | a new order reuses a valid authorization of the account for the same identifier, as Let's Encrypt does; `false` by default |
| `certificate_lifetime` | how long a certificate is valid, 90 days by default (backdated a minute when an hour or longer) |
| `processing_time` | how long finalize leaves an order `processing`, answered with a Retry-After; zero, the default, issues at once |
| `terms_of_service` | the URL of terms in the directory, which an account must agree to; empty by default |
| `require_external_account` | a new account needs an external account binding; `false` by default |
| `external_accounts` | the bindings taken, by key id ([external_account](external_account.md)) |
| `alternate_chains` | how many more chains each certificate has, its intermediate cross-signed by a root of each; 0 by default |
| `renewal_info` | renewalInfo (RFC 9773) in the directory; `true` by default |
| `profiles` | the profiles offered, by name, with their descriptions; none by default |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    net::acme::test_server ca(
        {.terms_of_service = "https://ca.example/terms", .alternate_chains = 1});
    net::acme::client acme(ca.directory_url(), net::acme::account_key());
    net::acme::directory d = acme.directory();
    println("{}", d.terms_of_service);
}
```

Output:

```text
https://ca.example/terms
```

## See also

- [test_server](test_server/README.md)
- [net::acme](README.md)
