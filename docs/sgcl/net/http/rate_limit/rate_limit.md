[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [rate_limit](README.md)

# sgcl::net::http::rate_limit::rate_limit

```cpp
rate_limit(double per_second, size_t burst);                      // (1)
rate_limit(double per_second, size_t burst, const options& o);    // (2)
```

1. Buckets of `per_second` tokens a second, `burst` at once, by the client's address.
2. The same with the key, the idle time and the most keys of the options ([rate_limit::options](../rate_limit-options.md)).

- (1–2) A rate of 0 lets each key make `burst` requests and none after (until its bucket is dropped as idle).

## Parameters

| Parameter | Description |
|---|---|
| `per_second` | the tokens a bucket gains a second; 0 or more |
| `burst` | the tokens it holds at most: the requests a key may make at once; 1 or more |
| `o` | the key, the idle time, the most keys |

## Complexity

Constant.

## Exceptions

`invalid_argument` for a burst of 0, or a rate below 0 or not a number.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::rate_limit once_each(0, 1, {.header = "X-API-Key"});
    net::http::server srv;
    srv.use(once_each);
    srv.route("GET /", [](net::http::request, net::http::response_writer w) { w.write("ok"); });
    for (const char* key : {"a", "b", "a"}) {
        auto req = net::http::test_request("GET", "/");
        req.set_header("X-API-Key", key);
        net::http::response_recorder rec;
        rec.serve(srv, req);
        println("{} {}", key, rec.status());
    }
}
```

Output:

```text
a 200
b 200
a 429
```

## See also

- [rate_limit](README.md)
