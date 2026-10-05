[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [agent](README.md)

# sgcl::net::ssh::agent::close

```cpp
expected<void, io::error> close() const noexcept;
```

The connection to the agent closed; the agent and its keys go on. A request after it is `io::errc::closed`.

## Parameters

None.

## Return value

Nothing.

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
    net::ssh::agent other = net::ssh::agent::connect("agent.sock");
    other.close();
    println("{}", other.list().error().is_closed());
    a.close();
    agent.process.kill();
    agent.wait();
}
```

Output:

```text
true
```

## See also

- [connect](connect.md)
- [sgcl::net::ssh::agent](README.md)
