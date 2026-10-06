[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [server_session](README.md)

# sgcl::net::ssh::server_session::window_changes

```cpp
async::receive_channel<void> window_changes() const noexcept;
```

A notification of every window change the client sends (RFC 4254 §6.7), for the handler that runs a program on a
terminal: received, the new size is [pty](pty.md)'s, which the handler gives the program's terminal — an
[io::pty](../../../io/pty/README.md)'s [resize](../../../io/pty/resize.md), the program's `SIGWINCH`. A burst of
changes the handler has not caught up with is one notification (the channel holds one), and the size read after it
is the newest. The channel is closed when the client closes the session or the connection ends, so that a loop
over it ends with the session. A session without a terminal gets no notification.

## Parameters

None.

## Return value

The receiving side of the session's channel ([async::receive_channel](../../../async/receive_channel/README.md)).

## Complexity

Constant.

## Exceptions

None.

## Example

A server that runs the session's command with `/bin/sh` on a pseudo-terminal of the client's size, as `sshd`
does, and resizes it with the client's window:

```cpp
#include "sgcl/async.h"
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"
#include "sgcl/net.h"

using namespace sgcl;

io::terminal_size size_of(const net::ssh::pty& p) {
    return {.rows = uint16_t(p.rows), .columns = uint16_t(p.columns)};
}

async::task<> follow_window(net::ssh::server_session s, io::pty term) {
    while (co_await s.window_changes().receive()) {
        term.resize(size_of(*s.pty()));
    }
}

async::task<> type_input(net::ssh::server_session s, io::pty term) {
    co_await io::async_copy(term, s.input());
}

async::task<> shell(net::ssh::server_session s) {
    io::pty term = io::open_pty(size_of(*s.pty())).value();
    io::command sh("/bin/sh", "-c", s.command());
    term.start(sh).value();
    async::go(type_input(s, term));
    async::go(follow_window(s, term));
    co_await io::async_copy(s.output(), term);  // the screen, until the shell ends
    co_await sh.async_wait();
    co_await s.async_exit(sh.state->exit_code());
}

int main() {
    net::ssh::server srv;
    srv.host_keys = {net::ssh::private_key::generate()};
    srv.check_password = [](const string& user, const string& password) { return password == "secret"; };
    srv.handle(shell);
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::ssh::session s = c.open_session();
    s.request_pty();
    s.exec("trap 'stty size; exit 3' WINCH; echo ready; while :; do sleep 0.05; done");
    io::buffered_reader screen(s.output());
    println("{}", screen.read_line().value().value());
    s.window_change(120, 40);
    print("{}", screen.read_all_text().value().replace("\r\n", "\n"));
    println("exit {}", s.wait()->code);
    srv.close();
}
```

Output:

```text
ready
40 120
exit 3
```

## See also

- [pty](pty.md): the terminal and its size
- [io::pty](../../../io/pty/README.md): a program on a pseudo-terminal
- [session::window_change](../session/window_change.md): the client's side
- [sgcl::net::ssh::server_session](README.md)
