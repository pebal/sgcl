[sgcl](../../../README.md) › [net](../../README.md) › [oidc](../README.md) › [provider](README.md)

# sgcl::net::oidc::provider::userinfo, async_userinfo

```cpp
expected<encoding::json, oauth2::error> userinfo(const oauth2::token& t, const string& subject = {}) const;    // (1)
async::task<expected<encoding::json, oauth2::error>> async_userinfo(oauth2::token t,                           // (2)
                                                                    string subject = {}) const noexcept;
```

The claims of the userinfo endpoint (Core §5.3) for the token's access token, sent as `Authorization: Bearer`. Given a
subject — the ID token's — the answer's `sub` must be it (Core §5.3.2: `invalid_userinfo` otherwise), so that the
claims are of the user who signed in. A provider without a userinfo endpoint is `invalid_argument` in the error's
`transport()`.

- (1) Block the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
  program, never a handler.
- (2) Return a task that does the same.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the token of the sign-in |
| `subject` | the ID token's `sub`; `""`: not checked |

## Return value

The claims, an [encoding::json](../../../encoding/json/README.md) object, or the [oauth2::error](../../oauth2/error/README.md).

## Complexity

One request.

## Exceptions

None: a failure is in the returned error.

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
    net::oauth2::token t;
    t.access_token = "at-1";
    auto me = provider->userinfo(t, "ann");
    println("{}", (*me)["name"].as_string(""));
    println("{}", provider->userinfo(t, "bob").error().code());
    op.close();
}
```

Output:

```text
Ann Example
invalid_userinfo
```

## See also

- [verify](verify.md)
- [provider](README.md)
