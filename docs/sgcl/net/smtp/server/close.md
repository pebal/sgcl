[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [server](README.md)

# sgcl::net::smtp::server::close

```cpp
void close() const;
```

At once: every listener and connection closed, the sessions in progress ended.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the number of connections.

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
    srv.close();
    println("{}", serving.wait().error().message().view().ends_with("server closed"));
}
```

Output:

```text
true
```

## See also

- [shutdown](shutdown.md)
- [server](README.md)
