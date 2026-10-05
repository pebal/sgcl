[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [agent](README.md)

# sgcl::net::ssh::agent::sign, async_sign

```cpp
expected<vector<byte>, io::error> sign(const public_key& key, const slice<const byte>& data) const;                                // (1)
async::task<expected<vector<byte>, io::error>> async_sign(const public_key& key, const slice<const byte>& data) const noexcept;    // (2)
```

The signature blob of `data` by the agent's key `key` (SIGN_REQUEST): its algorithm's name and its bytes, as
[private_key::sign](../private_key/sign.md) gives them, an RSA key's by rsa-sha2-512. The key stays in the agent.

1. On a thread.
2. In a task.

## Parameters

| Parameter | Description |
|---|---|
| `key` | one of the agent's keys |
| `data` | the data to sign |

## Return value

The signature blob. Or the [io::error](../../../io/error/README.md): `net::errc::ssh_request_refused` when the agent has no such key or refuses, the connection's.

## Complexity

A round trip and a signature.

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
    vector<byte> sig = a.sign(key.public_key(), "to be signed").value();
    println("{}", key.public_key().verify("to be signed", sig));
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

- [public_key::verify](../public_key/verify.md)
- [sgcl::net::ssh::agent](README.md)
