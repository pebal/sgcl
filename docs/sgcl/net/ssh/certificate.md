[sgcl](../../README.md) › [net](../README.md) › [ssh](README.md)

# sgcl::net::ssh::certificate

```cpp
#include "sgcl/net/ssh/keys.h"   // or "sgcl/net/ssh.h"

namespace sgcl::net::ssh {
    struct certificate {
        ssh::public_key key;
        certificate_type type = certificate_type::user;
        uint64_t serial = 0;
        string key_id;
        vector<string> principals;
        uint64_t valid_after = 0;
        uint64_t valid_before = 0;
        vector<pair<string, string>> critical_options;
        vector<pair<string, string>> extensions;
        ssh::public_key signature_key;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`net::ssh::certificate` is an OpenSSH certificate's fields (PROTOCOL.certkeys), as
[public_key::certificate](public_key/certificate.md) reads them from a certificate whose signature verifies: a key
signed by an authority for users or hosts of some names, for a time. A client takes a host certificate against the
`@cert-authority` lines of [known_hosts](known_hosts/README.md); a server a user certificate through its
`check_public_key`, with [authorized_keys](authorized_keys/README.md)'s `cert-authority` lines or the program's own
list of authorities. Go's `ssh.Certificate`. A plain struct; the module reads certificates and does not make them
(`ssh-keygen -s` does).

## Member objects

| Member | Description |
|---|---|
| `key` | the key certified, a plain key |
| `type` | what it is for, a user or a host ([certificate_type](certificate_type.md)) |
| `serial` | the authority's serial number of it |
| `key_id` | the authority's name of it (ssh-keygen's `-I`), what logs show |
| `principals` | the users or host names it is for; empty: any |
| `valid_after`, `valid_before` | its validity, in seconds since 1970; `0` and `UINT64_MAX` for always and forever |
| `critical_options` | names and values a server must understand (`force-command`, `source-address`) |
| `extensions` | names and values a server may take (`permit-pty`, `permit-port-forwarding` …) |
| `signature_key` | the authority's key, which signed it |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ssh.h"

using namespace sgcl;

int main() {
    net::ssh::public_key user = net::ssh::public_key::parse(io::read_text("tests/net/ssh/testdata/ed25519-cert.pub"));
    net::ssh::certificate c = user.certificate().value();
    println("{} {} {}", c.key_id, c.principals[0], c.principals[1]);
    println("{} {}", c.valid_after, c.valid_before == UINT64_MAX);
    for (auto& [name, value] : c.extensions) {
        println("{}", name);
    }
}
```

Output:

```text
alice-id alice admin
0 true
permit-X11-forwarding
permit-agent-forwarding
permit-port-forwarding
permit-pty
permit-user-rc
```

## See also

- [public_key::certificate](public_key/certificate.md)
- [known_hosts](known_hosts/README.md): host certificates; [authorized_keys::allows](authorized_keys/allows.md): user certificates
- [net::ssh](README.md)
