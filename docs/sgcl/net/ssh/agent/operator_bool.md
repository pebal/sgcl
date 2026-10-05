[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [agent](README.md)

# sgcl::net::ssh::agent::operator bool

```cpp
explicit operator bool() const noexcept;
```

Whether the handle has a connection: `false` for a default-constructed one.

## Parameters

None.

## Return value

Whether there is a connection.

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
    println("{} {}", (bool)none, (bool)a);
    a.close();
    agent.process.kill();
    agent.wait();
}
```

Output:

```text
false true
```

## See also

- [(constructor)](agent.md)
- [sgcl::net::ssh::agent](README.md)
