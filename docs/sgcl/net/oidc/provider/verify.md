[sgcl](../../../README.md) › [net](../../README.md) › [oidc](../README.md) › [provider](README.md)

# sgcl::net::oidc::provider::verify, async_verify

```cpp
expected<id_token, oauth2::error> verify(const string& raw, const verify_options& o) const;                  // (1)
async::task<expected<id_token, oauth2::error>> async_verify(string raw, verify_options o) const noexcept;    // (2)
```

An ID token checked as Core §3.1.3.7 asks, with [crypto::jose](../../../crypto/jose.md): the signature by a key of the
provider's set — the key the token's `kid` names, with the algorithm the key allows, never `none` — then `iss` the
provider's [issuer](issuer.md), `aud` holding the client and `azp` the client when there are several audiences or an
`azp`, `exp` not past and `iat` present within the leeway, `sub` present, and the nonce the request's. The key set is
fetched at the first call, and again (at most once a minute) for a `kid` it lacks. Any failure is `invalid_id_token`
with what failed; a key set that cannot be fetched or read is that error.

- (1) Block the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
  program, never a handler.
- (2) Return a task that does the same.

## Parameters

| Parameter | Description |
|---|---|
| `raw` | the ID token, compact (the token response's `id_token`) |
| `o` | the client, the nonce and the leeway ([verify_options](../provider-verify_options.md)) |

## Return value

The [id_token](../id_token.md), or the [oauth2::error](../../oauth2/error/README.md).

## Complexity

One signature verification; a request when the keys are fetched.

## Exceptions

None: a failure is in the returned error.

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
    println("{}", id->subject);
    println("{}", provider->verify(raw, {.client_id = "web", .nonce = "n-2"}).error().message());
    println("{}", provider->verify(raw, {.client_id = "mobile"}).has_value());
    op.close();
}
```

Output:

```text
ann
invalid_id_token: the nonce is not the request's
false
```

## See also

- [id_token](../id_token.md)
- [verify_options](../provider-verify_options.md)
- [provider](README.md)
