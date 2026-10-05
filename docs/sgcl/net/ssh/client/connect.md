[sgcl](../../../README.md) › [net](../../README.md) › [ssh](../README.md) › [client](README.md)

# sgcl::net::ssh::client::connect, async_connect

```cpp
static expected<client, io::error> connect(const string& address);                                    // (1)
static expected<client, io::error> connect(const string& address, const options& o);                  // (2)
static expected<client, io::error> connect(const net::connection& transport, const options& o);       // (3)
static async::task<expected<client, io::error>> async_connect(string address) noexcept;               // (4)
static async::task<expected<client, io::error>> async_connect(string address, options o) noexcept;    // (5)
static async::task<expected<client, io::error>> async_connect(net::connection transport,              // (6)
                                                              options o) noexcept;
```

A connection to the server at `address`, `"host:port"`, or over `transport`, a connection the program made (through a
proxy, a test's pair in memory): the version lines exchanged, the key exchange run, the server's host key checked,
the user authenticated; the client is returned only then.

- (1, 4) The default [options](../client-options.md): the user `$USER`, the key files `~/.ssh/id_ed25519`, `id_ecdsa`
  and `id_rsa` that load without a passphrase, then the keys of the agent at `$SSH_AUTH_SOCK`; the host key checked
  against `~/.ssh/known_hosts`; 30 s for the whole of it.
- (2, 5) The options `o`: the user, the keys and certificates, the agent, a password, keyboard-interactive answers,
  the host key's check, the timeout and the stop, a dial function, the algorithms, compression, the rekeying limits,
  keepalive, the agent's forwarding.
- (3, 6) Over `transport`, its remote endpoint the name known_hosts is asked about; the transport is closed when it
  fails.
- (1–3) Run on the scheduler and wait for it: for a thread, as [task::wait](../../../async/task/wait.md) is; a task
  awaits (4–6).

The methods are tried in the order publickey (each certificate before the key it certifies, each key signed and sent
at once), keyboard-interactive (when the options have its callback), password (when they have one), each only when
the server's list names it. An RSA key signs by rsa-sha2-512, or rsa-sha2-256 when the server's `server-sig-algs`
names it alone (RFC 8332); a server that names neither is offered rsa-sha2-512.

## Parameters

| Parameter | Description |
|---|---|
| `address` | the server's `"host:port"`; also the name known_hosts and the host key callback are asked about |
| `transport` | a connection to the server the program made |
| `o` | how it connects and authenticates ([options](../client-options.md)) |

## Return value

The client, authenticated. Or the [io::error](../../../io/error/README.md):

- of the dial (`ECONNREFUSED`, `net::errc::host_not_found` …), or the dial function's;
- `net::errc::ssh_handshake`: no SSH version line, a version other than 2.0, no algorithm in common (the message names
  the list), a host key whose signature does not verify, a strict key exchange broken;
- `net::errc::ssh_host_key_unknown`, `ssh_host_key_mismatch`, `ssh_host_key_revoked`, or the callback's error;
- `net::errc::ssh_auth_failed`: no method succeeded (the message names those tried and those the server takes), or the
  server ended it after too many failures;
- `net::errc::ssh_protocol` for a message that breaks the protocol, `net::errc::ssh_disconnected` for the server's
  disconnect (its reason in the message);
- `ETIMEDOUT` (`is_timeout()`) past `o.timeout`, `ECANCELED` for `o.stop`.

## Complexity

The dial, a round trip for the version lines, one for the key exchange (and its keys' derivation: ML-KEM, X25519 and a signature's check), two to four for the authentication.

## Exceptions

- (1–3) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (4–6) None.

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
    srv.handle([](net::ssh::server_session s) { (void)s.output().write("ran " + s.command()); });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.password = "secret";
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    println("{}", c.user());
    o.password = "wrong";
    auto refused = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    println("{}", refused.error().code() == net::errc::ssh_auth_failed);
    o.password = "secret";
    o.insecure_ignore_host_key = false;
    o.known_hosts = net::ssh::known_hosts();  // knows no host
    auto unknown = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    println("{}", unknown.error().code() == net::errc::ssh_host_key_unknown);
    srv.close();
}
```

Output:

```text
ann
true
true
```

## See also

- [options](../client-options.md)
- [known_hosts](../known_hosts/README.md): the host key's check
- [close](close.md)
- [sgcl::net::ssh::client](README.md)
