[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md)

# sgcl::net::ssh::session

```cpp
#include "sgcl/net/ssh/client.h"   // or "sgcl/net/ssh.h"

namespace sgcl::net::ssh {
    class session;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`net::ssh::session` is a session of a [client](../client/README.md) (RFC 4254 §6), Go's `ssh.Session`: a channel in
which the server runs one thing — a command ([exec](exec.md)), the user's shell ([shell](shell.md)) or a subsystem
([subsystem](subsystem.md), SFTP's) — after the requests that come before it: a terminal
([request_pty](request_pty.md)) and the environment ([set_env](set_env.md)). While it runs, its standard input is
an [io::writer](../../../io/writer/README.md) ([input](input.md)), its output and error
[io::reader](../../../io/reader/README.md)s ([output](output.md), [error_output](error_output.md)); the terminal's size
changes ([window_change](window_change.md)) and signals go to it ([signal](signal.md)); [wait](wait.md) gives how it
ended, its exit code or the signal that ended it.

A session is a handle of one word: a copy is the same session. Against Go, the streams are always there as streams
(no `Stdin` field to set before the start, no `StdinPipe` to take once), and [client::run](../client/run.md) is the
one-line form of `Output` and `CombinedOutput` together.

## Rules

- The requests (`request_pty`, `set_env`) come before `exec`, `shell` or `subsystem`, and one of those once; each waits
  for the server's answer, and a refusal is `net::errc::ssh_request_refused`. `window_change` and `signal` ask for no
  answer.
- The streams are the channel's: what is written to `input` waits for the server's window; what the program writes
  waits in the session until it is read, and once 2 MB wait unread, the server's program waits too. Read `output` and
  `error_output` as the program writes, from two tasks or threads when both may grow, or let [wait](wait.md) drop
  what is not read.
- [close_input](close_input.md) (or `input().close()`) sends the end of the input (EOF): a program that reads its
  input to its end, `cat` or `wc`, ends then.
- [close](close.md) ends the session at once; the server decides what becomes of its program (OpenSSH's sends it
  SIGHUP).
- The blocking forms are for a thread; a task awaits the `async_` forms.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](session.md) | no session |
| [set_env, async_set_env](set_env.md) | an environment variable for the program |
| [request_pty, async_request_pty](request_pty.md) | a pseudo-terminal for the program |
| [exec, async_exec](exec.md) | starts a command |
| [shell, async_shell](shell.md) | starts the user's shell |
| [subsystem, async_subsystem](subsystem.md) | starts a subsystem |
| [window_change, async_window_change](window_change.md) | the terminal's new size |
| [signal, async_signal](signal.md) | a signal to the program |
| [input](input.md) | the program's standard input |
| [output](output.md) | the program's standard output |
| [error_output](error_output.md) | the program's standard error |
| [close_input, async_close_input](close_input.md) | the end of the program's input |
| [wait, async_wait](wait.md) | how the program ended |
| [close](close.md) | ends the session |
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
    srv.handle([](net::ssh::server_session s) {
        string text = s.input().read_all_text().value();
        (void)s.output().write(to_string(text.size()) + " bytes");
        (void)s.exit(text.empty() ? 1 : 0);
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::ssh::session s = c.open_session();
    s.exec("wc -c");
    s.input().write("hello, world");
    s.close_input();
    println("{}", s.output().read_all_text().value());
    println("{}", s.wait()->code);
    srv.close();
}
```

Output:

```text
12 bytes
0
```

## See also

- [client::open_session](../client/open_session.md), [client::run](../client/run.md)
- [server_session](../server_session/README.md): the server's side of it
- [exit_status](../exit_status.md), [pty](../pty.md)
- [net::ssh](../README.md)
