[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [client](README.md)

# sgcl::net::ssh::client::run, async_run

```cpp
expected<run_result, io::error> run(const string& command) const;                                // (1)
async::task<expected<run_result, io::error>> async_run(const string& command) const noexcept;    // (2)
```

Runs `command` in a session of its own, as `ssh host command` does: the session opened, the command started, its
input closed at once, its output and error read whole as they come, the session waited for. Go's `Session.Output` and
`CombinedOutput` with the exit code kept: a code other than 0, or an end by a signal, is in the result's
[status](../exit_status.md), not an error.

1. On a thread, as [task::wait](../../../async/task/wait.md) is.
2. In a task, which holds no worker while it waits.

What runs the command is the server's: OpenSSH's runs it in the user's shell, the module's
[server](../server/README.md) hands it to its handler.

## Parameters

| Parameter | Description |
|---|---|
| `command` | the command, as the server's shell takes it |

## Return value

The command's standard output and standard error, and how it ended ([run_result](../run_result.md)). Or the
[io::error](../../../io/error/README.md): `net::errc::ssh_channel_refused` for a session the server refused (its limit
of sessions), `net::errc::ssh_request_refused` for a command it refused, the connection's error when it ended
meanwhile.

## Complexity

Three round trips (the session's open, the command's start, its end), and the output's bytes.

## Exceptions

- (1) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2) None.

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
        (void)s.output().write("out: " + s.command());
        (void)s.error_output().write("err");
        (void)s.exit(3);
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::ssh::run_result r = c.run("make");
    println("{} | {} | {}", r.out, r.err, r.status.code);
    srv.close();
}
```

Output:

```text
out: make | err | 3
```

## See also

- [open_session](open_session.md): the session's full form
- [run_result](../run_result.md), [exit_status](../exit_status.md)
- [sgcl::net::ssh::client](README.md)
