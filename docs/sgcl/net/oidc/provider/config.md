[sgcl](../../../README.md) › [net](../../README.md) › [oidc](../README.md) › [provider](README.md)

# sgcl::net::oidc::provider::config

```cpp
oauth2::config config(const string& client_id, const string& client_secret, const string& redirect_url,
                      const vector<string>& scopes = {}) const;
```

An [oauth2::config](../../oauth2/config/README.md) of the provider's [endpoints](endpoints.md) for a client: its id, secret
and redirect, the scope `openid` first and the scopes given after it, the provider's HTTP client. The sign-in is then the
config's [authorization_url](../../oauth2/config/authorization_url.md) with the nonce of
[request_secrets](../request_secrets/README.md) and its [exchange](../../oauth2/config/exchange.md), whose token's
`id_token` [verify](verify.md) checks.

## Parameters

| Parameter | Description |
|---|---|
| `client_id` | the client's id |
| `client_secret` | its secret; `""` for a public client |
| `redirect_url` | where the provider sends the user back |
| `scopes` | more scopes: `email`, `profile` |

## Return value

The config.

## Complexity

Linear in the number of scopes.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net/oauth2.h"
#include "sgcl/net/oidc.h"
#include "sgcl/net.h"
#include "sgcl/time.h"

using namespace sgcl;

int main() {
    auto key = crypto::jose::jwk::generate(crypto::jose::algorithm::es256, {.kid = "k1"});
    string keys = crypto::jose::jwk_set{key.public_key()}.to_json();
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    string issuer = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::http::server op;  // the provider, here in the program
    op.route("GET /.well-known/openid-configuration", [issuer](net::http::request, net::http::response_writer w) {
        w.write(R"({"issuer":")" + issuer + R"(","jwks_uri":")" + issuer + R"(/keys",)"
                R"("userinfo_endpoint":")" + issuer + R"(/userinfo","token_endpoint":")" + issuer + R"(/token"})");
    });
    op.route("GET /keys", [keys](net::http::request, net::http::response_writer w) { w.write(keys); });
    op.route("GET /userinfo", [](net::http::request, net::http::response_writer w) {
        w.write(R"({"sub":"ann","name":"Ann Example"})");
    });
    auto serving = async::spawn(op.async_serve(listener));
    auto provider = net::oidc::provider::discover(issuer);
    net::oauth2::config cfg = provider->config("web", "s3cret", "http://127.0.0.1:8080/callback", {"email"});
    println("{} {} {}", cfg.scopes[0], cfg.scopes[1], cfg.endpoints.token == issuer + "/token");
    op.close();
}
```

Output:

```text
openid email true
```

## See also

- [endpoints](endpoints.md)
- [request_secrets](../request_secrets/README.md)
- [provider](README.md)
