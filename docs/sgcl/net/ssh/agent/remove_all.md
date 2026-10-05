[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [agent](README.md)

# sgcl::net::ssh::agent::remove_all, async_remove_all

```cpp
expected<void, io::error> remove_all() const;                                // (1)
async::task<expected<void, io::error>> async_remove_all() const noexcept;    // (2)
```

Every key taken out of the agent (REMOVE_ALL_IDENTITIES), as `ssh-add -D` takes them.

1. On a thread.
2. In a task.

## Parameters

None.

## Return value

Nothing. Or the [io::error](../../../io/error/README.md): `net::errc::ssh_request_refused` when the agent refuses, the connection's.

## Complexity

A round trip.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;
using namespace std::chrono_literals;

int main() {
    io::command agent("/usr/bin/ssh-agent", "-D", "-a", "agent.sock");
    agent.start();
    while (!io::stat("agent.sock")) {
        async::sleep(10ms).wait();  // until the agent listens
    }
    net::ssh::agent a = net::ssh::agent::connect("agent.sock");
    a.add(net::ssh::private_key::generate());
    a.add(net::ssh::private_key::generate());
    a.remove_all();
    println("{}", a.list()->size());
    a.close();
    agent.process.kill();
    agent.wait();
}
```

Output:

```text
0
```

## See also

- [remove](remove.md)
- [sgcl::net::ssh::agent](README.md)
