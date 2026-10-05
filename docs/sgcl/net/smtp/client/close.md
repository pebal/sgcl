[sgcl](../../../README.md) › [net](../../README.md) › [smtp](../README.md) › [client](README.md)

# sgcl::net::smtp::client::close

```cpp
expected<void, io::error> close() const noexcept;
```

The connection closed without QUIT; the session ends.

## Parameters

None.

## Return value

Nothing, or the error of the close.

## Complexity

Constant.

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
    c.close();
    println("{}", c.noop().error().is_closed());
    srv.close();
    serving.wait();
}
```

Output:

```text
true
```

## See also

- [quit](quit.md)
- [client](README.md)
