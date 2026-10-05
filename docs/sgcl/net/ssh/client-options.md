[sgcl](../../README.md) › [net](../README.md) › [ssh](README.md) › [client](client/README.md) › options

# sgcl::net::ssh::client::options

```cpp
#include "sgcl/net/ssh/client.h"   // or "sgcl/net/ssh.h"

namespace sgcl::net::ssh {
    class client {
    public:
        struct options {
            string user;
            vector<ssh::private_key> keys;
            vector<ssh::public_key> certificates;
            bool agent = true;
            string password;
            function<vector<string>(const string&, const string&, const vector<ssh::prompt>&)> keyboard_interactive;
            optional<ssh::known_hosts> known_hosts;
            function<expected<void, io::error>(const string&, const ssh::public_key&)> host_key_callback;
            bool insecure_ignore_host_key = false;
            duration timeout = 30 * second;
            async::stop_token stop;
            function<async::task<expected<net::connection, io::error>>(const string&, async::stop_token)> dial;
            vector<string> kex;
            vector<string> host_key_algorithms;
            vector<string> ciphers;
            vector<string> macs;
            bool compression = false;
            uint64_t rekey_bytes = uint64_t(1) << 30;
            duration rekey_interval = 3600 * second;
            duration keepalive_interval = duration::zero();
            bool forward_agent = false;
            function<void(const string&)> banner;
        };
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`net::ssh::client::options` is how a [client](client/README.md) connects and authenticates: who it is, what it
proves it with, how it checks the server, how long it gives the handshake, which algorithms it offers. A plain struct,
its fields set by name; the forms of [connect](client/connect.md) without it take its defaults, which are `ssh`'s: the
user `$USER`, the user's key files and agent, the user's known_hosts.

## Member objects

| Member | Description |
|---|---|
| `user` | the user to authenticate as; empty by default: `$USER` |
| `keys` | the keys tried, in order; none by default: `~/.ssh/id_ed25519`, `id_ecdsa` and `id_rsa`, those that load without a passphrase |
| `certificates` | user certificates, each offered before the key it certifies; none by default |
| `agent` | the keys of the agent at `$SSH_AUTH_SOCK` tried after those; `true` by default |
| `password` | tried after the keys, when not empty and the server takes passwords |
| `keyboard_interactive` | answers to the server's questions (RFC 4256): the name, the instruction, the prompts; unset: the method not tried |
| `known_hosts` | the host keys trusted; none by default: [known_hosts::load()](known_hosts/load.md) of the user's file |
| `host_key_callback` | decides on the host key in known_hosts' place: the address and the key, an error refuses it; unset by default |
| `insecure_ignore_host_key` | any host key taken unchecked, for tests; `false` by default |
| `timeout` | the dial, the handshake and the authentication together, `ETIMEDOUT` past it; zero: none; 30 s by default |
| `stop` | ends the connect with `ECANCELED` when stopped; none by default |
| `dial` | how the connection is made, given the address and the stop; unset by default: [tcp::connect](../tcp/connect.md) |
| `kex`, `host_key_algorithms`, `ciphers`, `macs` | the algorithms offered, in order of preference; empty by default: the module's ([net::ssh](README.md)) |
| `compression` | zlib@openssh.com offered first, after the authentication; `false` by default |
| `rekey_bytes`, `rekey_interval` | a new key exchange after so many bytes either way or so long; 1 GB and an hour by default |
| `keepalive_interval` | keepalive@openssh.com sent when the server is silent this long, three unanswered end the connection; zero by default: none |
| `forward_agent` | the agent at `$SSH_AUTH_SOCK` forwarded to the sessions' servers (`ssh -A`); `false` by default |
| `banner` | hears the server's banner before the authentication (RFC 4252 §5.4); unset by default: it is dropped |

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
    srv.check_keyboard_interactive = [](const string& user, const vector<string>& answers) { return answers[0] == "42"; };
    srv.prompts = {net::ssh::prompt{"The answer: ", true}};
    srv.banner = "authorized use only\n";
    srv.handle([](net::ssh::server_session s) { (void)s.output().write("in"); });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.agent = false;
    o.keys = {};
    o.ciphers = {"aes256-gcm@openssh.com"};
    o.banner = [](const string& text) { print("banner: {}", text); };
    o.keyboard_interactive = [](const string& name, const string& instruction, const vector<net::ssh::prompt>& prompts) {
        println("asked: {}", prompts[0].text);
        return vector<string>{"42"};
    };
    o.host_key_callback = [](const string& address, const net::ssh::public_key& key) -> expected<void, io::error> {
        println("host key: {}", key.type_name());
        return {};
    };
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    println("{}", c.run("x")->out);
    srv.close();
}
```

Output:

```text
host key: ssh-ed25519
banner: authorized use only
asked: The answer: 
in
```

## See also

- [client::connect](client/connect.md)
- [known_hosts](known_hosts/README.md), [private_key](private_key/README.md), [agent](agent/README.md)
- [sgcl::net::ssh::client](client/README.md)
