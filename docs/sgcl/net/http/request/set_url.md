[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [request](README.md)

# sgcl::net::http::request::set_url

```cpp
request& set_url(const string& url) noexcept;
```

Sets the URL the request goes to, parsed now as the [constructor](request.md) parses it; a URL that does not parse is
the send's error, `net::errc::invalid_url`. What the request already holds — its method, fields and body — stays. A
[reverse_proxy](../reverse_proxy/README.md)'s `rewrite` sends a request elsewhere with it.

## Parameters

| Parameter | Description |
|---|---|
| `url` | the URL, `http://` or `https://` |

## Return value

`*this`.

## Complexity

Linear in the length of the URL.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"

using namespace sgcl;

int main() {
    net::http::test_server ts([](net::http::request r, net::http::response_writer w) {
        w.write(r.url().path() + " " + r.header("X-Kept") + "\n");
    });
    net::http::request req("GET", ts.url() + "/first");
    req.set_header("X-Kept", "yes");
    req.set_url(ts.url() + "/second");
    print("{}", ts.client().send(req)->text().value());
    req.set_url("not a url");
    println("{}", ts.client().send(req).error().code().message());
}
```

Output:

```text
/second yes
invalid URL
```

## See also

- [url](url.md): the URL it holds
- [(constructor)](request.md): the method and the URL
- [sgcl::net::http::request](README.md)
