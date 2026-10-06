[sgcl](../../../README.md) › [net](../../README.md) › [oidc](../README.md) › [provider](README.md)

# sgcl::net::oidc::provider::discover, async_discover

```cpp
static expected<provider, oauth2::error> discover(const string& issuer);                                         // (1)
static expected<provider, oauth2::error> discover(const string& issuer, const http::client& c);                  // (2)
static async::task<expected<provider, oauth2::error>> async_discover(string issuer) noexcept;                    // (3)
static async::task<expected<provider, oauth2::error>> async_discover(string issuer, http::client c) noexcept;    // (4)
```

The provider of an issuer (Discovery 1.0 §4): the document at `issuer + "/.well-known/openid-configuration"` read,
its `issuer` held to the one asked for (`invalid_issuer` otherwise) and its `jwks_uri` required (`invalid_metadata`).
The keys are not fetched yet.

- (1, 3) With a client of the default settings; (2, 4) with the client given (its TLS roots, its proxy), which the
  provider then uses for its keys and userinfo, and gives its [config](config.md).
- (1–2) Block the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
  program, never a handler.
- (3–4) Return a task that does the same.

## Parameters

| Parameter | Description |
|---|---|
| `issuer` | the provider's issuer, `https://accounts.example.com` |
| `c` | the HTTP client |

## Return value

The provider, or the [oauth2::error](../../oauth2/error/README.md).

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
    println("{}", net::oidc::provider::discover(issuer + "/other").error().code());
    op.close();
}
```

Output:

```text
true
http_status
```

## See also

- [issuer](issuer.md)
- [provider](README.md)
