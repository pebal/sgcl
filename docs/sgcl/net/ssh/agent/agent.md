[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [agent](README.md)

# sgcl::net::ssh::agent::agent

```cpp
agent() noexcept;                 // (1)
agent(const agent&) = default;    // (2), implicitly declared
```

1. No agent: an operation on it is a contract violation; `operator bool` is `false`.
2. The same connection: a copy shares it.

## Parameters

None.

## Complexity

Constant.

## Exceptions

None.

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
    net::ssh::agent none;
    net::ssh::agent copy = a;
    println("{} {}", (bool)none, copy.list()->size());
    a.close();
    agent.process.kill();
    agent.wait();
}
```

Output:

```text
false 0
```

## See also

- [connect](connect.md)
- [sgcl::net::ssh::agent](README.md)
