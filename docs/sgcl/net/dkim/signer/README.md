[sgcl](../../../README.md) › [net](../../README.md) › [dkim](../README.md)

# sgcl::net::dkim::signer

```cpp
#include "sgcl/net/dkim.h"   // or "sgcl/net/smtp.h"

namespace sgcl::net::dkim {
    class signer;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::dkim::signer` is a signing identity of DKIM: the domain that signs (`d=`), the selector under which its
key is published (`s=`), and the private key, RSA or Ed25519, read from PEM or made new. [record](record.md) is the
TXT record to publish at [record_name](record_name.md), [private_key_pem](private_key_pem.md) the key to keep;
[sign](sign.md) signs with it, and so does an [smtp::client](../../smtp/client/README.md) whose options name it.

## Rules

- A handle of one word whose state is made by the constructor: a copy is the same signer, sharing the key.
- The key lives outside the managed heap, never copied; its words are zeroed when the last handle's object goes.
- The domain and the selector are kept in lower case.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](signer.md) | a signer of a key in PEM, or a copy that shares one |
| `(destructor)` | drops the handle |
| `operator=` | the handle of another signer |
| [from_pem](from_pem.md) | a signer of a key in PEM, a failure as an error |
| [generate](generate.md) | a signer of a new key |
| [domain](domain.md) | `d=` |
| [selector](selector.md) | `s=` |
| [algorithm](algorithm.md) | the key's algorithm |
| [record_name](record_name.md) | where the key's record is published |
| [record](record.md) | the TXT record to publish |
| [private_key_pem](private_key_pem.md) | the private key as PEM |
| [sign](sign.md) | the message with a `DKIM-Signature` field put first |
| [operator==](operator_cmp.md) | whether two handles are the same signer |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/io.h"
#include "sgcl/net/smtp.h"

using namespace sgcl;

int main() {
    auto s = net::dkim::signer::generate("example.com", "s2026");
    println("{} IN TXT \"{}...\"", s.record_name(), s.record().view().substr(0, 24));
    io::write_file("dkim.pem", s.private_key_pem());

    net::dkim::signer again("example.com", "s2026", io::read_file("dkim.pem").value());
    println("{}", again.record() == s.record());
}
```

Output:

```text
s2026._domainkey.example.com IN TXT "v=DKIM1; k=rsa; p=MIIBIj..."
true
```

## See also

- [sign](sign.md)
- [dkim](../README.md)
