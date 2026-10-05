[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [reverse_proxy](README.md)

# sgcl::net::http::reverse_proxy::reverse_proxy

```cpp
explicit reverse_proxy(const string& url);                          // (1)
reverse_proxy(const string& url, const options& o);                 // (2)
explicit reverse_proxy(const vector<string>& backends);             // (3)
reverse_proxy(const vector<string>& backends, const options& o);    // (4)
reverse_proxy(const reverse_proxy& other) = default;                // (5)
```

Constructs a proxy. A backend is an `http://` or `https://` URL: its scheme, host and port are where the requests
go, its path is joined before each request's with one slash between (`http://app/api` and `/users` make
`/api/users`), and its query before each request's, joined by `&`. The options are read now: a change of them, or of
the client given in them, after the construction does not reach the proxy. With a health path, the checks start
now, on the scheduler, and go on while any copy of the proxy lives (or until [close](close.md)).

1. A proxy to one backend, with the default options.
2. The same with `o` ([options](../reverse_proxy-options.md)).
3. A proxy balancing the backends, in turn by default.
4. The same with `o`: the policy, health, retries.
5. A copy shares the backends, their counts and the client's pool. There is no move of its own: a moved-from proxy is
   the same proxy, as a moved-from [tracked_ptr](../../../core/tracked_ptr/README.md) still points.

## Parameters

| Parameter | Description |
|---|---|
| `url` | the backend's URL |
| `backends` | the backends' URLs, at least one |
| `o` | the hooks, the client, flushing, the fields, balancing, health, retries |
| `other` | the proxy to share |

## Complexity

- (1–4) Linear in the number of backends: each URL parsed once.
- (5) Constant.

## Exceptions

- (1–4) `invalid_argument` for no backend, or one that is not an `http://` or `https://` URL with a host.
- (5) None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::test_server one([](net::http::request r, net::http::response_writer w) {
        w.write("one saw " + r.url().request_target() + "\n");
    });
    net::http::server front;
    front.route("/", net::http::reverse_proxy(one.url() + "/base?key=1"));
    net::http::test_server proxy(front);
    print("{}", proxy.client().get(proxy.url() + "/page?q=2")->text().value());
    try {
        net::http::reverse_proxy bad("ftp://example.com/");
    } catch (const invalid_argument& e) {
        println("{}", e.what());
    }
}
```

Output:

```text
one saw /base/page?key=1&q=2
http::reverse_proxy: a backend is an http:// or https:// URL
```

## See also

- [options](../reverse_proxy-options.md)
- [operator()](operator_call.md): the handler
- [sgcl::net::http::reverse_proxy](README.md)
