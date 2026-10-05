[sgcl](../../README.md) › [net](../README.md) › [acme](README.md)

# sgcl::net::acme::manager::options

```cpp
#include "sgcl/net/acme/manager.h"   // or "sgcl/net/acme.h"

namespace sgcl::net::acme {
    class manager {
    public:
        struct options {
            string directory_url = string(lets_encrypt_url);
            string cache;
            vector<string> contact;
            bool accept_terms = false;
            optional<acme::external_account> external_account;
            optional<account_key> key;
            function<bool(const string&)> host_policy;
            vector<string> challenges = {string("tls-alpn-01"), string("http-01")};
            function<async::task<expected<void, io::error>>(const string&, const string&)>
                dns_publish;
            function<async::task<expected<void, io::error>>(const string&, const string&)>
                dns_cleanup;
            duration renew_before = duration::zero();
            key_algorithm certificate_key = key_algorithm::es256;
            string default_name;
            acme::client::options client;
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

Where a [manager](manager/README.md)'s certificates come from and how: Go's `autocert.Manager` fields as plain values.
A designated initializer names what differs: `{.cache = "certs", .accept_terms = true}`.

## Member objects

| Object | Description |
|---|---|
| `directory_url` | the CA's directory; Let's Encrypt's production one by default, `lets_encrypt_staging_url` its staging one |
| `cache` | the directory of the account's key and the certificates (made at 0700, files of 0600); empty, the default, keeps them in memory alone |
| `contact` | the account's contact, `"mailto:admin@example.com"`; none by default |
| `accept_terms` | the CA's terms of service agreed to; `false` by default, which a CA with terms refuses (Let's Encrypt does: `errc::malformed`) |
| `external_account` | the binding of a CA that requires one ([external_account](external_account.md)) |
| `key` | the account's key; none, the default: the cache's, else a new P-256 key kept there |
| `host_policy` | whether a name that is not among the manager's names may have a certificate (Go's `HostPolicy`); empty, the default, allows none |
| `challenges` | the types solved, in order of preference: each authorization by the first the CA offers, a failure tried again with the next in a new order; `tls-alpn-01` then `http-01` by default (`http-01` needs [http_handler](manager/http_handler.md) on port 80); `dns-01` needs `dns_publish`. A wildcard is obtained by `dns-01` alone |
| `dns_publish` | dns-01: the TXT record of a name ([client::dns01_name](client/dns01_name.md)) set to a value, before the CA is told to look; its error ends the attempt |
| `dns_cleanup` | dns-01: the record removed once the authorization is settled; empty leaves it |
| `renew_before` | a renewal this long before the certificate expires; zero, the default: when the CA's renewal information suggests, else at two thirds of the lifetime |
| `certificate_key` | the kind of the certificates' keys ([key_algorithm](key_algorithm.md)); P-256 by default |
| `default_name` | the name of a hello without SNI (a client of an IP address); empty, the default, refuses it |
| `client` | the ACME client's ([client::options](client-options.md)): its HTTP client, retries, polling |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/acme.h"

using namespace sgcl;

int main() {
    // dns-01 through a publisher of the program's own (a map here, a DNS provider's API at work)
    tracked_ptr<map<string, string>> records = make_tracked<map<string, string>>();
    net::acme::test_server ca({.lookup_txt = [records](const string& name)
                                   -> async::task<expected<vector<string>, io::error>> {
        co_return vector<string>{(*records)[name]};
    }});
    net::acme::manager::options o;
    o.directory_url = ca.directory_url();
    o.accept_terms = true;
    o.dns_publish = [records](const string& name, const string& value)
        -> async::task<expected<void, io::error>> {
        println("TXT {}", name);
        (*records)[name] = value;
        co_return expected<void, io::error>();
    };
    net::acme::manager certificates({"*.example.com"}, o);
    net::tls::identity id = certificates.certificate("www.example.com");
    println("{}", id.certificates()[0].dns_names()[0]);
    certificates.close();
}
```

Output:

```text
TXT _acme-challenge.example.com
*.example.com
```

## See also

- [manager](manager/README.md)
- [net::acme](README.md)
