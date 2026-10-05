[sgcl](../../../README.md) › [net](../../README.md) › [http](../README.md) › [proxy](README.md)

# sgcl::net::http::proxy::from_environment

```cpp
static proxy from_environment() noexcept;
```

The proxy of the environment, as curl reads it:

- `http` from `http_proxy`, in lower case only: a program run by CGI gets a request's `Proxy` field as `HTTP_PROXY`
  (httpoxy), so curl does not read it, and neither does this; Go reads it outside CGI;
- `https` from `https_proxy`, or `HTTPS_PROXY`;
- `all_proxy`, or `ALL_PROXY`, for either of the two whose own is not set;
- `no_proxy` from `no_proxy`, or `NO_PROXY`.

The lower case is read first, as curl does (Go reads the upper case first), and a variable set to the empty text is
taken as not set. A [client](../client/README.md) calls it when it is made, so a client made after the environment
changed sees the change, and one made before does not; [http::download](../download.md)'s client is made at the
first download.

## Parameters

None.

## Return value

The proxy the environment names; none when it names none.

## Complexity

Linear in the size of the environment.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/http.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    io::unsetenv("http_proxy");
    io::unsetenv("all_proxy");
    io::unsetenv("ALL_PROXY");
    io::setenv("https_proxy", "http://proxy.example:3128");
    io::setenv("no_proxy", "localhost,.internal");
    io::setenv("HTTP_PROXY", "http://not-read.example:1");

    net::http::proxy p = net::http::proxy::from_environment();
    println("'{}' '{}' '{}'", p.http, p.https, p.no_proxy);

    io::setenv("ALL_PROXY", "socks5h://127.0.0.1:1080");
    println("'{}'", net::http::proxy::from_environment().http);
}
```

Output:

```text
'' 'http://proxy.example:3128' 'localhost,.internal'
'socks5h://127.0.0.1:1080'
```

## See also

- [(constructor)](proxy.md): none, or one proxy
- [io::getenv](../../../io/getenv.md): the environment
- [proxy](README.md)
