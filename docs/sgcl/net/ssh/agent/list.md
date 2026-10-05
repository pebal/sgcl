[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [agent](README.md)

# sgcl::net::ssh::agent::list, async_list

```cpp
expected<vector<public_key>, io::error> list() const;                                // (1)
async::task<expected<vector<public_key>, io::error>> async_list() const noexcept;    // (2)
```

The keys the agent holds, their public halves with their comments, in the agent's order (REQUEST_IDENTITIES). Keys of
a kind not read here (a security key's) are passed over.

1. On a thread.
2. In a task.

## Parameters

None.

## Return value

The keys. Or the [io::error](../../../io/error/README.md): `net::errc::ssh_request_refused` when the agent refuses (a locked agent), `net::errc::ssh_protocol` for an answer that breaks the protocol, the connection's.

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
    a.add(net::ssh::private_key::load("tests/net/ssh/testdata/p256"));
    a.add(net::ssh::private_key::load("tests/net/ssh/testdata/rsa"));
    vector<net::ssh::public_key> keys = a.list().value();
    for (net::ssh::public_key& key : keys) {
        println("{} {}", key.type_name(), key.comment());
    }
    a.close();
    agent.process.kill();
    agent.wait();
}
```

Output:

```text
ecdsa-sha2-nistp256 test-p256
ssh-rsa test-rsa
```

## See also

- [add](add.md)
- [sgcl::net::ssh::agent](README.md)
