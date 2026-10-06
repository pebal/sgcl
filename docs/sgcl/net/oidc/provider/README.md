[sgcl](../../../README.md) › [net](../../README.md) › [oidc](../README.md)

# sgcl::net::oidc::provider

```cpp
#include "sgcl/net/oidc/oidc.h"   // or "sgcl/net/oidc.h"

namespace sgcl::net::oidc {
    class provider;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

An OpenID provider (Discovery 1.0), Go's `oidc.Provider`: its metadata read from the issuer's
`/.well-known/openid-configuration`, the [oauth2::config](../../oauth2/config/README.md) of its endpoints, ID tokens
verified with its keys, its userinfo endpoint.

## Rules

- Made by [discover](discover.md); the issuer the document names must be the one asked for, character for character.
- The key set (`jwks_uri`) is fetched at the first verification, and again when a token names a `kid` the set lacks —
  the provider rotated its keys — at most once a minute.
- A handle of one word: copies share the metadata and the keys; any number of tasks may verify at once.

## Member types

| Type | Definition |
|---|---|
| [verify_options](../provider-verify_options.md) | the client, the nonce and the leeway of a verification |

## Member functions

| Function | Description |
|---|---|
| [discover, async_discover](discover.md) | the provider of an issuer (static) |
| [issuer](issuer.md) | the issuer |
| [metadata](metadata.md) | the discovery document |
| [endpoints](endpoints.md) | the OAuth 2.0 endpoints of the metadata |
| [config](config.md) | an oauth2::config of the provider for a client |
| [verify, async_verify](verify.md) | an ID token checked |
| [userinfo, async_userinfo](userinfo.md) | the claims of the userinfo endpoint |

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
    auto id = provider->verify(raw, {.client_id = "web", .nonce = "n-1"});
    println("signed in: {}", id->subject);
    op.close();
}
```

Output:

```text
signed in: ann
```

## See also

- [oauth2::config](../../oauth2/config/README.md)
- [id_token](../id_token.md)
