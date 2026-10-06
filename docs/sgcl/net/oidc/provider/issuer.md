[sgcl](../../../README.md) › [net](../../README.md) › [oidc](../README.md) › [provider](README.md)

# sgcl::net::oidc::provider::issuer

```cpp
string issuer() const noexcept;
```

The issuer the provider was discovered by: the `iss` its ID tokens must carry.

## Parameters

None.

## Return value

The issuer.

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
    println("{}", provider->issuer() == issuer);
    op.close();
}
```

Output:

```text
true
```

## See also

- [discover](discover.md)
- [provider](README.md)
