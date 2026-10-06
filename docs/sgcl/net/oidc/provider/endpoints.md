[sgcl](../../../README.md) › [net](../../README.md) › [oidc](../README.md) › [provider](README.md)

# sgcl::net::oidc::provider::endpoints

```cpp
oauth2::endpoints endpoints() const noexcept;
```

The OAuth 2.0 endpoints the metadata names ([oauth2::endpoints](../../oauth2/endpoints.md)): authorization, token, device
authorization, revocation, introspection; `""` for one the provider does not have.

## Parameters

None.

## Return value

The endpoints.

## Complexity

Constant.

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
    net::oauth2::endpoints e = provider->endpoints();
    println("{} {}", e.token == issuer + "/token", e.revocation.empty());
    op.close();
}
```

Output:

```text
true true
```

## See also

- [config](config.md)
- [provider](README.md)
