[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [session](README.md)

# sgcl::net::ssh::session::wait, async_wait

```cpp
expected<exit_status, io::error> wait() const;                                // (1)
async::task<expected<exit_status, io::error>> async_wait() const noexcept;    // (2)
```

How the program ended (RFC 4254 §6.10), once the server closed the session: its exit code, or the signal that ended
it with whether it dumped core and the server's message. From the call on, what the program still writes and what of
its output and error waits unread is dropped, its window given back, so that a program whose output nobody reads is
never held up on a full window; read the streams first when their bytes matter. Go's `Session.Wait`, the code kept in
the value rather than an `ExitError`.

## Parameters

None.

## Return value

How it ended ([exit_status](../exit_status.md)): `code` the exit code, or -1 with `signal` set when a signal ended it
(or the server sent neither). Or the [io::error](../../../io/error/README.md) of the connection when it ended first.

## Complexity

Constant: woken by the close.

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
        if (s.command() == "crash") {
            (void)s.exit_signal("SEGV", true, "segmentation fault");
        } else {
            (void)s.exit(7);
        }
    });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    net::ssh::session a = c.open_session();
    a.exec("false");
    println("{}", a.wait()->code);
    net::ssh::session b = c.open_session();
    b.exec("crash");
    net::ssh::exit_status st = b.wait();
    println("{} {} {} {}", st.code, st.signal, st.core_dumped, st.message);
    srv.close();
}
```

Output:

```text
7
-1 SEGV true segmentation fault
```

## See also

- [exit_status](../exit_status.md)
- [client::run](../client/run.md)
- [sgcl::net::ssh::session](README.md)
