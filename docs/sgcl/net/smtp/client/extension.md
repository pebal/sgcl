[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [client](README.md)

# sgcl::net::smtp::client::extension

```cpp
string extension(const string& keyword) const noexcept;
```

The parameters of an extension EHLO named: `"35882577"` of SIZE, `"PLAIN LOGIN"` of AUTH.

## Parameters

| Parameter | Description |
|---|---|
| `keyword` | the keyword, any case |

## Return value

The parameters; `""` for none, or for an extension not named.

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
    println("{}", c.extension("SIZE"));
    c.quit();
    srv.close();
    serving.wait();
}
```

Output:

```text
33554432
```

## See also

- [client](README.md)
