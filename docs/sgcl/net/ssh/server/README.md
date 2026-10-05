[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md)

# sgcl::net::ssh::server

```cpp
#include "sgcl/net/ssh/server.h"   // or "sgcl/net/ssh.h"

namespace sgcl::net::ssh {
    class server;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`net::ssh::server` is an SSH server built the way [http::server](../../http/server/README.md) is: its host keys and
settings are fields, a [handler](handle.md) gets each session, [serve](serve.md) listens and runs a task per
connection, [shutdown](shutdown.md) or [close](close.md) ends it. Go's `ssh.ServerConfig` and the loop a program
writes around `ssh.NewServerConn`, in one. The user is authenticated by the program's callbacks, one per method:
`check_password`, `check_public_key` (with [authorized_keys::allows](../authorized_keys/allows.md) for OpenSSH's file
and its certificate authorities) and `check_keyboard_interactive`; a method is offered when its callback is set. A
session is handed to the handler as a [server_session](../server_session/README.md) once the client started a command,
a shell or a subsystem, with its terminal, its environment and its streams; the handler is the program, and the
session ends when it returns. Port forwarding both ways is the program's to allow, by `allow_direct_tcpip` and
`allow_tcpip_forward`.

Nothing is run by the server on its own: no system login, no shell, no PAM, no privilege separation. What a user may
do is what the handler does.

## Rules

- A handler is a function of a `server_session` that returns `void`, run on a thread of the blocking pool, where it
  uses the blocking forms (`s.output().write(...)`), or `async::task<>`, run as a task that awaits the `async_` forms.
  It runs in a task of its own per session; one that throws sends exit status 1, and `on_error` hears of it.
- When the handler returns, the session ends: the exit status (0 unless [exit](../server_session/exit.md) or
  [exit_signal](../server_session/exit_signal.md) sent one), the end of its output and the close go out together.
- The fields are read when `serve` is called, for the connections that `serve` accepts; the handler is shared by the
  copies of the server.
- `login_grace_time` (120 s) bounds the handshake and the authentication of a connection; `max_auth_tries` (6)
  failures end it, as OpenSSH's MaxAuthTries; `max_sessions` (10) bounds its sessions at once.
- A user certificate is checked before `check_public_key` sees it: its signature, the user type, its validity now and
  the user among its principals; the callback decides on the authority that signed it.
- `serve` and `shutdown` block the calling thread, the connections served on the scheduler meanwhile; a task awaits
  `async_serve` and `async_shutdown`.

## Member objects

| Member | Description |
|---|---|
| `host_keys` | the server's keys, one of each kind at most used; the host key algorithms offered are those of the keys given; none: `serve` refuses (`EINVAL`) |
| `host_certificates` | host certificates of those keys, offered as their own algorithms (`ssh-ed25519-cert-v01@openssh.com` …); none by default |
| `check_password` | whether a user's password lets them in; unset: the method not offered |
| `check_public_key` | whether a user's key (a certificate's, once it holds for the user) lets them in; unset: not offered |
| `check_keyboard_interactive` | whether a user's answers to `prompts` let them in; unset: not offered |
| `prompts` | keyboard-interactive's questions, `Password: ` without echo by default |
| `no_client_auth` | the method `none` lets anyone in; `false` by default |
| `max_auth_tries` | the failures before the connection is ended, 6 by default |
| `login_grace_time` | the handshake and the authentication of a connection, 120 s by default; zero: none |
| `max_sessions` | a connection's sessions at once, 10 by default |
| `max_packet` | a channel's largest data packet, 32 KB by default |
| `allow_direct_tcpip` | whether a user's connection through the server to a host and port is made (the server dials it); unset: refused |
| `allow_tcpip_forward` | whether a user's listener on the server at a host and port is made; unset: refused |
| `kex`, `host_key_algorithms`, `ciphers`, `macs` | the algorithms offered, in order of preference; empty: the module's |
| `compression` | zlib@openssh.com taken when a client offers it first; `false` by default |
| `rekey_bytes`, `rekey_interval` | a new key exchange after so many bytes either way (1 GB) or so long (an hour) |
| `keepalive_interval` | keepalive@openssh.com sent when a client is silent this long, three unanswered end it; zero by default: none |
| `banner` | a text sent to the client before the authentication (RFC 4252 §5.4); empty by default: none |
| `on_error` | a handler's exception, an accept's failure; a line on stderr by default |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](server.md) | a server with no sessions served yet |
| [handle](handle.md) | the handler of the sessions |
| [serve, async_serve](serve.md) | listens and serves |
| [shutdown, async_shutdown](shutdown.md) | the listeners closed, the connections waited for |
| [close](close.md) | everything closed at once |

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
    srv.check_password = [](const string& user, const string& password) {
        return user == "ann" && password == "secret";
    };
    srv.handle([](net::ssh::server_session s) -> async::task<> {
        co_await s.output().async_write("hello, " + s.user() + ": " + s.command());
        co_await s.async_exit(0);
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    println("{}", c.run("uptime")->out);
    srv.close();
}
```

Output:

```text
hello, ann: uptime
```

## See also

- [server_session](../server_session/README.md): what the handler gets
- [authorized_keys](../authorized_keys/README.md): OpenSSH's file for `check_public_key`
- [client](../client/README.md): the other side
- [http::server](../../http/server/README.md): the same shape
- [net::ssh](../README.md)
