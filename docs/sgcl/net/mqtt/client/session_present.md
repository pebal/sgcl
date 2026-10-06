[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md) › [client](README.md)

# sgcl::net::mqtt::client::session_present

```cpp
bool session_present() const noexcept;
```

Whether the broker had a session of the client id when it connected (`clean_start` false and a session it kept): its subscriptions go on, and the messages queued while the client was away come now.

## Parameters

None.

## Return value

`true` for a session the broker had.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/mqtt.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::mqtt::broker b;
    net::listener l = net::tcp::listen("127.0.0.1:0");
    auto serving = async::spawn(b.async_serve(l));
    string url = string::concat("mqtt://", l.local_endpoint().to_string());
    net::mqtt::client::options o;
    o.client_id = "dev-1";
    o.clean_start = false;
    o.session_expiry = std::chrono::minutes(5);
    net::mqtt::client first = net::mqtt::client::connect(url, o).value();
    first.disconnect();
    net::mqtt::client again = net::mqtt::client::connect(url, o).value();
    println("{} {}", first.session_present(), again.session_present());
    again.disconnect();
    b.close();
    serving.wait();
}
```

Output:

```text
false true
```

## See also

- [connect](connect.md)
- [client](README.md)
