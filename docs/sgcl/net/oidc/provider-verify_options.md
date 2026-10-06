[sgcl](../../README.md) › [net](../README.md) › [oidc](README.md) › [provider](provider/README.md) › verify_options

# sgcl::net::oidc::provider::verify_options

```cpp
#include "sgcl/net/oidc/oidc.h"   // or "sgcl/net/oidc.h"

namespace sgcl::net::oidc {
    class provider {
    public:
        struct verify_options {
            string client_id;
            string nonce;
            duration leeway = std::chrono::minutes(1);
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::oidc::provider::verify_options` is what an ID token is held to by [verify](provider/verify.md): a plain
struct, its fields set by name.

## Member objects

| Member | Description |
|---|---|
| `client_id` | the client: `aud` must hold it, and `azp` be it when there are several audiences or an `azp` |
| `nonce` | the nonce of the authorization request ([request_secrets](request_secrets/README.md)); `""`: not checked |
| `leeway` | the clocks' skew allowed for `exp` and `iat`. A minute by default |

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
    auto claims = encoding::json::object({{"iss", issuer}, {"sub", "ann"}, {"aud", "web"},
                                          {"exp", time::now().unix() - 30}, {"iat", time::now().unix() - 90}});
    string raw = crypto::jose::jwt::sign(claims, key);  // expired 30 seconds ago
    auto provider = net::oidc::provider::discover(issuer);
    println("{}", provider->verify(raw, {.client_id = "web"}).has_value());
    println("{}", provider->verify(raw, {.client_id = "web", .leeway = std::chrono::seconds(5)}).has_value());
    op.close();
}
```

Output:

```text
true
false
```

## See also

- [provider::verify](provider/verify.md)
