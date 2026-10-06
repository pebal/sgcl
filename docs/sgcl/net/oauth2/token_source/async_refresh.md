[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md) › [token_source](README.md)

# sgcl::net::oauth2::token_source::async_refresh

```cpp
async::task<expected<oauth2::token, error>> async_refresh() const noexcept;
```

The token refreshed now, whatever its expiry, and held from then on: for a resource server that refused it
(`invalid_token`) before it expired, the server having revoked it. The source's [client](client.md) does this by
itself. Returns a task; a thread of the program waits for it with `.wait()`.

## Parameters

None.

## Return value

The new [token](../token/README.md), or the [error](../error/README.md) of the refresh.

## Complexity

One exchange with the server.

## Exceptions

None: every failure is in the returned error.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net/oauth2.h"
#include "sgcl/net.h"

#include <atomic>

using namespace sgcl;

int main() {
    std::atomic<int> asked{0};
    net::http::server as;  // the authorization server, here in the program
    as.route("POST /token", [&asked](net::http::request req, net::http::response_writer w) {
        w.write(R"({"access_token":"at-)" + to_string(++asked) + R"(","expires_in":3600})");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(as.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::oauth2::config cfg;
    cfg.endpoints.token = base + "/token";
    net::oauth2::token_source tokens = cfg.source();
    println("{}", tokens.token()->access_token);
    println("{}", tokens.async_refresh().wait()->access_token);
    println("{}", tokens.token()->access_token);
    as.close();
}
```

Output:

```text
at-1
at-2
at-2
```

## See also

- [token](token.md)
- [token_source](README.md)
