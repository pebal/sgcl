[sgcl](../../README.md) › [net](../README.md) › [ssh](README.md)

# sgcl::net::ssh::prompt

```cpp
#include "sgcl/net/ssh/types.h"   // or "sgcl/net/ssh.h"

namespace sgcl::net::ssh {
    struct prompt {
        string text;
        bool echo = false;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`net::ssh::prompt` is a question of keyboard-interactive authentication (RFC 4256): its text and whether the answer may
be shown as it is typed (`false` for a password or a one-time code). A [server](server/README.md) asks its `prompts`;
a [client](client/README.md)'s `keyboard_interactive` callback gets them and gives the answers in their order.

## Member objects

| Member | Description |
|---|---|
| `text` | the question, as it is shown (`Password: `) |
| `echo` | whether the answer is shown as typed; `false` by default |

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
    srv.prompts = {net::ssh::prompt{"User code: ", true}, net::ssh::prompt{"PIN: ", false}};
    srv.check_keyboard_interactive = [](const string& user, const vector<string>& answers) {
        return answers.size() == 2 && answers[0] == "XK-17" && answers[1] == "1234";
    };
    srv.handle([](net::ssh::server_session s) { (void)s.output().write("verified"); });
    net::listener l = net::tcp::listen("127.0.0.1:0");
    async::go(srv.async_serve(l));

    net::ssh::client::options o;
    o.user = "ann";
    o.agent = false;
    o.keys = {};
    o.insecure_ignore_host_key = true;  // the server's key was made just now
    o.keyboard_interactive = [](const string& name, const string& instruction, const vector<net::ssh::prompt>& prompts) {
        for (const net::ssh::prompt& p : prompts) {
            println("{} (echo {})", p.text, p.echo);
        }
        return vector<string>{"XK-17", "1234"};
    };
    net::ssh::client c = net::ssh::client::connect(l.local_endpoint().to_string(), o);
    println("{}", c.run("x")->out);
    srv.close();
}
```

Output:

```text
User code:  (echo true)
PIN:  (echo false)
verified
```

## See also

- [server](server/README.md): `prompts`, `check_keyboard_interactive`
- [client::options](client-options.md): `keyboard_interactive`
- [net::ssh](README.md)
