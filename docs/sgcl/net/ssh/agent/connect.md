[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [agent](README.md)

# sgcl::net::ssh::agent::connect, async_connect

```cpp
static expected<agent, io::error> connect(const string& path = {});                         // (1)
static async::task<expected<agent, io::error>> async_connect(string path = {}) noexcept;    // (2)
```

A connection to the agent at the unix socket `path`; empty, the one `$SSH_AUTH_SOCK` names, as ssh finds it.

1. On a thread.
2. In a task.

## Parameters

| Parameter | Description |
|---|---|
| `path` | the agent's socket; empty: `$SSH_AUTH_SOCK` |

## Return value

The agent. Or the [io::error](../../../io/error/README.md): `io::errc::not_found` when the path is empty and `$SSH_AUTH_SOCK` is not set, the socket's (`ENOENT`, `ECONNREFUSED`).

## Complexity

A connection on a unix socket.

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
    println("{}", a.list().has_value());
    println("{}", net::ssh::agent::connect("no-agent.sock").error().is_not_found());
    a.close();
    agent.process.kill();
    agent.wait();
}
```

Output:

```text
true
true
```

## See also

- [unix_domain::connect](../../unix_domain/connect.md)
- [sgcl::net::ssh::agent](README.md)
