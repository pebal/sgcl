[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md)

# sgcl::net::ssh::client

```cpp
#include "sgcl/net/ssh/client.h"   // or "sgcl/net/ssh.h"

namespace sgcl::net::ssh {
    class client {
    public:
        struct options;   // the user, the keys, the password, the host key's check, the algorithms, the limits
    };
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`net::ssh::client` is a connection to an SSH server, Go's `ssh.Client`: [connect](connect.md) dials the server (or
takes a connection the program made), runs the key exchange, checks the server's host key and authenticates the user,
and returns the client only then. Over it, [run](run.md) runs a command in one line and gives its output and how it
ended; [open_session](open_session.md) opens a [session](../session/README.md) for a command, a shell or a subsystem
with a terminal and the three standard streams; [dial](dial.md) opens a connection through the server
(`ssh -L`'s way), a [net::connection](../../connection/README.md) as any other, and [listen](listen.md) a listener on
the server's side (`ssh -R`'s way) whose connections come here.

A client is a handle of one word: a copy is the same connection, shared by every session, forwarded connection and
request made over it, from any number of tasks and threads at once. Against Go, the host key is checked against the
user's known_hosts unless the program says otherwise (Go asks for a callback and has no default), the user's key
files and agent are used when no key is given (as `ssh` does), and a command's exit code other than 0 is a value of
[run_result](../run_result.md), not an error.

## Rules

- [close](close.md) ends the connection at once: every session, forwarded connection and listener over it ends with
  `io::errc::closed`, as does every call after it; the server's disconnect or a failure end them the same way, with
  their error, which [wait](wait.md) gives.
- The connection's channels share it: a channel's data goes in packets of at most 32 KB within the window the peer
  gives, so a slow reader of one channel does not hold the others up.
- A key exchange starts again after `options::rekey_bytes` either way (1 GB) or `rekey_interval` (an hour), or when
  the server asks; the calls in progress wait for its end.
- The blocking forms run the work on the scheduler and wait for it, so they are for a thread, as
  [task::wait](../../../async/task/wait.md) is; a task awaits the `async_` forms.

## Member types

| Type | Definition |
|---|---|
| [options](../client-options.md) | how a client connects and authenticates |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](client.md) | no connection |
| [connect, async_connect](connect.md) | a connection to a server, authenticated (static) |
| [run, async_run](run.md) | a command run, its output and how it ended |
| [open_session, async_open_session](open_session.md) | a new session |
| [dial, async_dial](dial.md) | a connection made by the server (direct-tcpip) |
| [listen, async_listen](listen.md) | a listener on the server's side (tcpip-forward) |
| [keepalive, async_keepalive](keepalive.md) | whether the server still answers |
| [wait, async_wait](wait.md) | until the connection ends |
| [close](close.md) | ends the connection |

#### Observers

| Function | Description |
|---|---|
| [is_closed](is_closed.md) | whether the connection ended |
| [host_key](host_key.md) | the server's host key |
| [server_version](server_version.md) | the server's version line |
| [user](user.md) | the user authenticated |
| [operator bool](operator_bool.md) | whether there is a connection |

## Example

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::ssh::server srv;
    srv.host_keys = {net::ssh::private_key::generate()};
    srv.check_password = [](const string& user, const string& password) { return password == "secret"; };
    srv.handle([](net::ssh::server_session s) { (void)s.output().write("hello, " + s.user()); });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::ssh::run_result r = c.run("whoami");
    println("{} ({})", r.out, r.status.code);
    c.close();
    srv.close();
}
```

Output:

```text
hello, ann (0)
```

## See also

- [session](../session/README.md): a command, a shell or a subsystem with its streams
- [server](../server/README.md): the other side
- [known_hosts](../known_hosts/README.md), [private_key](../private_key/README.md), [agent](../agent/README.md)
- [net::ssh](../README.md)
