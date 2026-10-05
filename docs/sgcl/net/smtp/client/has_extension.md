[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [client](README.md)

# sgcl::net::smtp::client::has_extension

```cpp
bool has_extension(const string& keyword) const noexcept;
```

Whether EHLO named the extension (after STARTTLS, the second EHLO); a server that took only HELO names none.

## Parameters

| Parameter | Description |
|---|---|
| `keyword` | the keyword, any case |

## Return value

`true` or `false`.

## Complexity

Linear in the number of extensions.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::smtp::server srv;
    srv.hostname = "mx.example";
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(srv.async_serve(l));
    string url = string::concat("smtp://", l.local_endpoint().to_string());
    net::smtp::client c = net::smtp::client::connect(url);
    println("{} {}", c.has_extension("chunking"), c.has_extension("STARTTLS"));
    c.quit();
    srv.close();
    serving.wait();
}
```

Output:

```text
true false
```

## See also

- [client](README.md)
