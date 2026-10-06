[sgcl](../../README.md) › [net](../README.md) › [oidc](README.md)

# sgcl::net::oidc::id_token

```cpp
#include "sgcl/net/oidc/oidc.h"   // or "sgcl/net/oidc.h"

namespace sgcl::net::oidc {
    struct id_token {
        string issuer;
        string subject;
        vector<string> audience;
        time::datetime expiry;
        time::datetime issued_at;
        string nonce;
        encoding::json claims;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

An ID token [verify](provider/verify.md) checked (Core §2): what a client signs the user in by. `subject` with `issuer`
is the user's identity at the provider, stable and never reassigned; an e-mail address in the claims is not.

## Member objects

| Member | Description |
|---|---|
| `issuer` | `iss`: the provider |
| `subject` | `sub`: the user |
| `audience` | `aud`: the clients it was issued to, one string or a list |
| `expiry` | `exp` |
| `issued_at` | `iat` |
| `nonce` | `nonce`: the authorization request's; `""` when the token has none |
| `claims` | every claim, for `email`, `name` and the provider's own |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/crypto.h"
#include "sgcl/io.h"
#include "sgcl/encoding.h"
#include "sgcl/net/http.h"
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
    auto claims = encoding::json::object({{"iss", issuer}, {"sub", "ann"}, {"aud", "web"}, {"nonce", "n-1"},
                                          {"exp", time::now().unix() + 3600}, {"iat", time::now().unix()}});
    string raw = crypto::jose::jwt::sign(claims, key);
    auto provider = net::oidc::provider::discover(issuer);
    net::oidc::id_token id = provider->verify(raw, {.client_id = "web", .nonce = "n-1"}).value();
    println("{} {} {}", id.subject, id.audience[0], id.nonce);
    op.close();
}
```

Output:

```text
ann web n-1
```

## See also

- [provider::verify](provider/verify.md)
