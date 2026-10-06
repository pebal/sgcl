[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md)

# sgcl::net::ssh::server_session

```cpp
#include "sgcl/net/ssh/server.h"   // or "sgcl/net/ssh.h"

namespace sgcl::net::ssh {
    class server_session;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`net::ssh::server_session` is a session as a [server](../server/README.md)'s handler gets it: what the client asked
for — a command ([command](command.md)), the user's shell or a subsystem ([subsystem](subsystem.md)), as
[kind](kind.md) says, a terminal ([pty](pty.md)), the environment ([env](env.md)) — and the streams of the program
the handler is: the client's input to read ([input](input.md)), the output and the error to write
([output](output.md), [error_output](error_output.md)). The handler ends the program with a status of its own
([exit](exit.md)) or a signal's ([exit_signal](exit_signal.md)); without one, its return is status 0. The client's
agent is reached through [agent](agent.md) when the client forwards it.

A session is a handle of one word: a copy is the same session, and a handle passed into a task keeps it, also past
the handler's return (its streams are closed then).

## Rules

- The handler runs once the client started the session; what it asked before (`pty`, `env`) is there from the start,
  and a terminal's size follows its window changes, a signal its [last_signal](last_signal.md).
- The output waits for the client's window: a write returns once the client's window took it; what the client
  writes waits in the session until the handler reads it.
- [stop](stop.md) is stopped when the client closes the session or the connection ends: a handler that runs long
  watches it.
- The blocking forms are for a handler returning `void` (it runs on the blocking pool); a handler returning
  `async::task<>` awaits the `async_` forms.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](server_session.md) | no session |
| [exit, async_exit](exit.md) | the exit status sent |
| [exit_signal, async_exit_signal](exit_signal.md) | an end by a signal sent |
| [agent, async_agent](agent.md) | the client's agent, when it forwards it |

#### Observers

| Function | Description |
|---|---|
| [user](user.md) | the user authenticated |
| [kind](kind.md) | what the session runs |
| [command](command.md) | exec's command |
| [subsystem](subsystem.md) | the subsystem's name |
| [pty](pty.md) | the terminal asked for |
| [window_changes](window_changes.md) | a notification of every window change |
| [env](env.md) | the variables the client set |
| [last_signal](last_signal.md) | the last signal the client sent |
| [stop](stop.md) | stopped when the session ends |
| [remote_endpoint](remote_endpoint.md) | the client's address |
| [input](input.md) | the client's input |
| [output](output.md) | the program's standard output |
| [error_output](error_output.md) | the program's standard error |
| [operator bool](operator_bool.md) | whether there is a session |

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
    srv.handle([](net::ssh::server_session s) -> async::task<> {
        if (s.kind() != net::ssh::session_kind::exec) {
            co_await s.error_output().async_write("commands only\n");
            co_await s.async_exit(2);
            co_return;
        }
        co_await s.output().async_write(s.user() + " ran " + s.command());
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    println("{}", c.run("ls")->out);
    net::ssh::session shell = c.open_session();
    shell.shell();
    print("{}", shell.error_output().read_all_text().value());
    println("{}", shell.wait()->code);
    srv.close();
}
```

Output:

```text
ann ran ls
commands only
2
```

## See also

- [server::handle](../server/handle.md)
- [session](../session/README.md): the client's side of it
- [net::ssh](../README.md)
