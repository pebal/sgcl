[sgcl](../../../README.md) › [net](../../README.md) › [oauth2](../README.md) › [token_source](README.md)

# sgcl::net::oauth2::token_source::token, async_token

```cpp
expected<oauth2::token, error> token() const;                                // (1)
async::task<expected<oauth2::token, error>> async_token() const noexcept;    // (2)
```

The token now: the one the source holds while it is [valid](../token/valid.md), else a new one — of its refresh token
([config::refresh](../config/refresh.md)), or for a source of credentials a new
[client_credentials](../config/client_credentials.md) token — which it then holds. Calls at once wait for one refresh
and all take its token; a failed refresh is the error of each, and the next call tries again.

- (1) Blocks the calling thread: the exchange runs on the scheduler and the thread waits for it. For a thread of the
  program, never a handler.
- (2) Returns a task that does the same: a handler writes `co_await cfg.async_token(...)`.

## Parameters

None.

## Return value

The [token](../token/README.md), or the [error](../error/README.md) of the refresh.

## Complexity

Constant while the token is valid; one exchange with the server when it is not.

## Exceptions

None: every failure is in the returned error.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
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
        ++asked;
        w.write(R"({"access_token":"at-1","expires_in":3600})");
    });
    net::listener listener = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(as.async_serve(listener));
    string base = "http://127.0.0.1:" + to_string(listener.local_endpoint().port());
    net::oauth2::config cfg;
    cfg.endpoints.token = base + "/token";
    net::oauth2::token_source tokens = cfg.source();
    for (int i : range(3)) {
        println("{} {}", i, tokens.token()->access_token);
    }
    println("asked {} time(s)", asked.load());
    as.close();
}
```

Output:

```text
0 at-1
1 at-1
2 at-1
asked 1 time(s)
```

## See also

- [async_refresh](async_refresh.md)
- [token::valid](../token/valid.md)
- [token_source](README.md)
