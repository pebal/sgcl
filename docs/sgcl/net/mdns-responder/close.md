[sgcl](../../README.md) › [net](../README.md) › [mdns](../mdns/README.md) › [responder](README.md)

# sgcl::net::mdns::responder::close, async_close

```cpp
void close() const;                                // (1)
async::task<void> async_close() const noexcept;    // (2)
```

Closes the responder: every record of its services and of its host said goodbye to (sent with TTL 0, RFC 6762
§10.1), then its sockets closed. A second close does nothing; a [publish](publish.md) after it is
`io::errc::closed`. A responder dropped without a close closes its sockets when the collector finds it, silently.

1. Waits on the calling thread for the goodbyes: for a thread, as [task::wait](../../async/task/wait.md) is (debug
   builds assert).
2. The same for a task.

## Parameters

None.

## Return value

None.

## Complexity

Linear in the services; a goodbye of each on each interface and family.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

async::task<> serve(net::mdns::options o) {
    auto r = co_await net::mdns::responder::async_start(o);
    println("{}", r->host_name());
    co_await r->async_close();
    co_await r->async_close();  // a second close: nothing
    println("closed");
}

int main() {
    net::mdns::options o;
    auto all = net::interfaces();
    for (auto& i : *all) {
        if (i.loopback) {
            o.interfaces.push_back(i);
        }
    }
    o.host = "docs-close";
    async::run(serve(o));
}
```

Output:

```text
docs-close.local.
closed
```

## See also

- [remove](remove.md): one service withdrawn
- [start](start.md): the beginning
- [sgcl::net::mdns::responder](README.md)
