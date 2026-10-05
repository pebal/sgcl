[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [agent](README.md)

# sgcl::net::ssh::agent::add, async_add

```cpp
expected<void, io::error> add(const private_key& key, duration lifetime = duration::zero()) const;                                // (1)
async::task<expected<void, io::error>> async_add(const private_key& key, duration lifetime = duration::zero()) const noexcept;    // (2)
```

The key given to the agent with its comment (ADD_IDENTITY), as `ssh-add` gives it: held until removed or the agent
ends, or for `lifetime` (ADD_ID_CONSTRAINED, `ssh-add -t`), whole seconds.

1. On a thread.
2. In a task.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key |
| `lifetime` | how long the agent holds it; zero: until removed |

## Return value

Nothing. Or the [io::error](../../../io/error/README.md): `net::errc::ssh_request_refused` when the agent refuses it, the connection's.

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
    a.add(net::ssh::private_key::generate().with_comment("for a minute"), 60 * second);
    println("{}", a.list()->front().comment());
    a.close();
    agent.process.kill();
    agent.wait();
}
```

Output:

```text
for a minute
```

## See also

- [remove](remove.md)
- [sgcl::net::ssh::agent](README.md)
