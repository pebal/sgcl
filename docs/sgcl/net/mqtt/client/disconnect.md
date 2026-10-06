[sgcl](../../../README.md) › [net](../../README.md) › [mqtt](../README.md) › [client](README.md)

# sgcl::net::mqtt::client::disconnect, async_disconnect

```cpp
expected<void, io::error> disconnect() const;
async::task<expected<void, io::error>> async_disconnect() const noexcept;
```

DISCONNECT with the reason 0x00, normal: the broker drops the will; then the connection closed and the session ended for this client. The broker keeps the session for its expiry.

`disconnect` waits on the calling thread; a task awaits `async_disconnect`.

## Parameters

None.

## Return value

Nothing; the session's end when it had ended already.

## Complexity

One packet.

## Exceptions

- `disconnect`: `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- `async_disconnect`: none.

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
    net::mqtt::client c = net::mqtt::client::connect(url).value();
    println("{}", c.disconnect().has_value());
    println("{}", c.is_connected());
    b.close();
    serving.wait();
}
```

Output:

```text
true
false
```

## See also

- [close](close.md)
- [client](README.md)
