[sgcl](../../README.md) › [net](../README.md) › ssh

# sgcl::net::ssh

```cpp
#include "sgcl/net/ssh.h"   // namespace sgcl::net::ssh
```

SSH ([RFC 4251](https://www.rfc-editor.org/rfc/rfc4251) to [RFC 4254](https://www.rfc-editor.org/rfc/rfc4254)), both
sides, as Go's `golang.org/x/crypto/ssh` gives it, over the module's [connections](../connection/README.md). A
[client](client/README.md) connects, checks the server's host key against [known_hosts](known_hosts/README.md) (or a
callback of the program's), authenticates the user by a key, a password or keyboard-interactive answers, and then
runs commands in one line ([run](client/run.md)) or in [sessions](session/README.md) with a terminal, an environment,
signals and the three standard streams as [io](../../io/README.md) streams; it opens connections through the server
([dial](client/dial.md), a [net::connection](../connection/README.md) like any other) and listens on the server's side
([listen](client/listen.md), a [net::listener](../listener/README.md) whose connections come back here). A
[server](server/README.md) is built as [http::server](../http/server/README.md) is: host keys, a callback per method of
authentication, limits, and a handler that gets each session — a command, a shell, a subsystem — as a
[server_session](server_session/README.md), with what the client asked for and its streams, and decides what it does:
nothing is run by the server on its own.

The protocol of today, and nothing older: the key exchanges mlkem768x25519-sha256 (the post-quantum hybrid, first by
default), curve25519-sha256, ecdh-sha2-nistp256, -nistp384 and -nistp521, diffie-hellman-group16-sha512 and -group14-sha256;
the ciphers chacha20-poly1305@openssh.com, aes128-gcm@openssh.com and aes256-gcm@openssh.com, aes128-ctr, aes192-ctr
and aes256-ctr with hmac-sha2-256 and hmac-sha2-512 and their encrypt-then-MAC forms; host and user keys of Ed25519,
ECDSA P-256, P-384 and P-521 and RSA (signing rsa-sha2-512 and rsa-sha2-256, never SHA-1's ssh-rsa), and OpenSSH's
certificates over them; strict key exchange (OpenSSH's answer to Terrapin, CVE-2023-48795), rekeying, zlib@openssh.com
compression after the authentication, the agent and its forwarding. Keys are read and written in OpenSSH's own file
format, encrypted with a passphrase or not, and in PEM's ([private_key](private_key/README.md),
[public_key](public_key/README.md)); an [agent](agent/README.md) is reached at `$SSH_AUTH_SOCK`. Interoperability was
tested against OpenSSH 10.2: the client against `sshd` and `ssh-agent`, `ssh` against the server, every algorithm
both ways, forwarding both ways, certificates, rekeying. **The implementation has not been through an independent
cryptographic audit.**

## The rules

1. The handshake and the authentication come before the client. [connect](client/connect.md) returns once the key
   exchange is done, the host key checked and the user authenticated; a failure is an
   [io::error](../../io/error/README.md) and no client. A connection the program handed to `connect` is closed when it
   fails.
2. The host key is checked by default. Against known_hosts (the user's `~/.ssh/known_hosts` unless the options give
   a set), a host certificate against its `@cert-authority` lines, or by the program's `host_key_callback`; an
   unknown key is `ssh_host_key_unknown`, another key for the host `ssh_host_key_mismatch`, a `@revoked` one
   `ssh_host_key_revoked`. Taking any key unchecked is a named option, `insecure_ignore_host_key`, for tests.
3. A connection carries many channels at once, from many tasks and threads: sessions, forwarded connections,
   requests. One task reads it and never waits on a writer; what it owes the peer is written by whoever writes next,
   so a peer that writes while it reads is always read.
4. Flow control is the protocol's: a channel's data waits for the peer's window (2 MB, as OpenSSH's), and what comes
   waits in the channel until it is read, the window given back as it is. A session's output that nobody reads holds
   the remote program up once the window is full; [wait](session/wait.md) drops what is not read, so that waiting for
   a program never hangs on its output.
5. A new key exchange starts after 1 GB either way or an hour (both configurable), or when the peer asks; the
   writers wait for its end, the readers do not notice it.
6. Every call that waits has two forms, as every call of the module: `c.run(...)` on a thread, `co_await
   c.async_run(...)` in a task. The blocking forms run the work on the scheduler and wait for it, so they are for a
   thread, as [task::wait](../../async/task/wait.md) is.
7. Errors are values, `expected<T, io::error>`, the codes of [errc](../errc.md) (`ssh_handshake`,
   `ssh_host_key_unknown`, `ssh_host_key_mismatch`, `ssh_host_key_revoked`, `ssh_auth_failed`, `ssh_protocol`,
   `ssh_disconnected`, `ssh_channel_refused`, `ssh_request_refused`), the system's for the transport, crypto's for a
   key file (`malformed`, `unsupported`, `authentication` for a missing or wrong passphrase).
8. Private keys live in unmanaged memory, never copied (the copies of a handle share them) and zeroed when the last
   handle is gone; so are the keys of a connection, its packets' buffers and every secret of a key exchange.
9. Not here: X11 forwarding, GSSAPI, host-based authentication, security keys (sk-*), DSA, the CBC ciphers and SHA-1
   anywhere, OpenSSH's host key rotation (hostkeys-00@openssh.com is refused), signing certificates (ssh-keygen -s
   makes them; the module reads and checks them).

## Classes

| Class | Header | Description |
|---|---|---|
| [agent](agent/README.md) | `agent.h` | an SSH agent at a unix socket: its keys listed, signatures, keys added and removed |
| [authorized_keys](authorized_keys/README.md) | `authorized_keys.h` | the keys a server lets users in with, OpenSSH's file with its options |
| [authorized_keys::entry](authorized_keys-entry/README.md) | `authorized_keys.h` | a line of authorized_keys: its key and its options |
| [certificate](certificate.md) | `keys.h` | an OpenSSH certificate's fields |
| [client](client/README.md) | `client.h` | an SSH client connection: sessions, commands, forwarding both ways |
| [client::options](client-options.md) | `client.h` | how a client connects and authenticates |
| [exit_status](exit_status.md) | `types.h` | how a remote program ended: its code or its signal |
| [known_hosts](known_hosts/README.md) | `known_hosts.h` | the host keys a client trusts, OpenSSH's file |
| [private_key](private_key/README.md) | `keys.h` | a private key, read, written and made |
| [prompt](prompt.md) | `types.h` | a question of keyboard-interactive authentication |
| [pty](pty.md) | `types.h` | a pseudo-terminal: its type, size and modes |
| [public_key](public_key/README.md) | `keys.h` | a public key or certificate: its text, blob, fingerprint |
| [run_result](run_result.md) | `types.h` | what a command run in one line gave |
| [server](server/README.md) | `server.h` | an SSH server: host keys, authentication by callbacks, a handler of sessions |
| [server_session](server_session/README.md) | `server.h` | a session as the server's handler gets it |
| [session](session/README.md) | `client.h` | a client's session: a command, a shell or a subsystem with its streams |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [certificate_type](certificate_type.md) | `keys.h` | what a certificate is for: a user or a host |
| [key_type](key_type.md) | `keys.h` | the kinds of key: Ed25519, ECDSA P-256, P-384 and P-521, RSA |
| [session_kind](session_kind.md) | `server.h` | what a session runs: a command, a shell, a subsystem |

## See also

- [net](../README.md): the connections, the listeners and [errc](../errc.md)
- [tls](../tls/README.md): the other secure channel of the module
- [crypto](../../crypto/README.md): the algorithms under it
- RFC 4250–4254, 4256, 4344, 5647, 5656, 6668, 8268, 8308, 8332, 8709, 8731; OpenSSH's PROTOCOL,
  PROTOCOL.certkeys, PROTOCOL.chacha20poly1305, PROTOCOL.key; draft-miller-ssh-agent;
  `tests/net/ssh/`, against OpenSSH
