[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [agent](README.md)

# sgcl::net::ssh::agent::remove, async_remove

```cpp
expected<void, io::error> remove(const public_key& key) const;                                // (1)
async::task<expected<void, io::error>> async_remove(const public_key& key) const noexcept;    // (2)
```

The key taken out of the agent (REMOVE_IDENTITY), as `ssh-add -d` takes it.

1. On a thread.
2. In a task.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key's public half |

## Return value

Nothing. Or the [io::error](../../../io/error/README.md): `net::errc::ssh_request_refused` when the agent does not hold it, the connection's.

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
    net::ssh::private_key key = net::ssh::private_key::generate();
    a.add(key);
    a.remove(key.public_key());
    println("{} {}", a.list()->size(), a.remove(key.public_key()).error().code() == net::errc::ssh_request_refused);
    a.close();
    agent.process.kill();
    agent.wait();
}
```

Output:

```text
0 true
```

## See also

- [remove_all](remove_all.md)
- [sgcl::net::ssh::agent](README.md)
