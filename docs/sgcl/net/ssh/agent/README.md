[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md)

# sgcl::net::ssh::agent

```cpp
#include "sgcl/net/ssh/agent.h"   // or "sgcl/net/ssh.h"

namespace sgcl::net::ssh {
    class agent;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`net::ssh::agent` is a connection to an SSH agent (draft-miller-ssh-agent): `ssh-agent`, a password manager's agent,
the agent a server forwards from the client ([server_session::agent](../server_session/agent.md)). The agent holds
private keys and never gives them out: it lists their public halves ([list](list.md)), signs with them
([sign](sign.md)), takes new keys for a time or for good ([add](add.md)) and lets them go ([remove](remove.md),
[remove_all](remove_all.md)). A [client](../client/README.md) authenticates with the keys of the agent at
`$SSH_AUTH_SOCK` by itself, and forwards it to the server when its options ask. Go's `agent.ExtendedAgent` of
`golang.org/x/crypto/ssh/agent`, the client's side.

A handle of one word over its connection: copies are the same connection, which takes a request and its answer at a
time.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](agent.md) | no agent |
| [connect, async_connect](connect.md) | the agent at a socket (static) |
| [list, async_list](list.md) | the keys it holds |
| [sign, async_sign](sign.md) | a signature by one of its keys |
| [add, async_add](add.md) | a key given to it |
| [remove, async_remove](remove.md) | a key taken out |
| [remove_all, async_remove_all](remove_all.md) | every key taken out |
| [close](close.md) | the connection closed |
| [operator bool](operator_bool.md) | whether there is a connection |

## Example

A private agent of the program's own, started for it and stopped after.

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
    a.add(net::ssh::private_key::load("tests/net/ssh/testdata/ed25519"));
    vector<net::ssh::public_key> keys = a.list().value();
    for (net::ssh::public_key& key : keys) {
        println("{} {}", key.fingerprint(), key.comment());
    }
    a.close();
    agent.process.kill();
    agent.wait();
}
```

Output:

```text
SHA256:PN89yHvZV5qRnQP3eclDnzJi7J8IfYfMitxf9jaYMn0 test-ed25519
```

## See also

- [client::options](../client-options.md): `agent`, `forward_agent`
- [server_session::agent](../server_session/agent.md)
- [net::ssh](../README.md)
