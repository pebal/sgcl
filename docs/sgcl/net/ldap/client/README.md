[sgcl](../../../README.md) › [net](../../README.md) › [ldap](../README.md)

# sgcl::net::ldap::client

```cpp
#include "sgcl/net/ldap/client.h"   // or "sgcl/net/ldap.h"

namespace sgcl::net::ldap {
    class client;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::ldap::client` is a session with an LDAP server: [connect](connect.md) dials and takes TLS as the
[options](../client-options.md) say, [bind](bind.md) authenticates, then entries are [searched](search.md),
[added](add.md), [modified](modify.md), [renamed](rename.md), [compared](compare.md) and [removed](remove.md).
`ldap3.Connection` of Python or `ldap.Conn` of go-ldap in one object, the filters read before they are sent.

## Rules

- A handle of one word, as a connection is: a copy is the same session.
- Operations may run at once, from several tasks: each is sent with an id of its own and waits for its own
  answer. The operation that finds no one reading the connection reads it itself and hands the others their
  messages, so that one operation at a time costs no task beside it.
- A referral (result 10) is the operation's error, its URLs in [result_of](../result_of.md); a search's
  references to other servers are in its [result](../search_result.md). Neither is followed.
- After [unbind](unbind.md), [close](close.md), a Notice of Disconnection or a failure of the connection, every
  call fails with the error that ended the session.

## Member types

| Type | Definition |
|---|---|
| [options](../client-options.md) | how a client talks to its server |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](client.md) | no session, or a copy of one |
| `(destructor)` | drops the handle |
| `operator=` | the handle of another session |
| [connect, async_connect](connect.md) | a session with a server |
| [bind, async_bind](bind.md) | a simple bind: a DN and its password |
| [bind_plain, async_bind_plain](bind_plain.md) | a SASL bind of PLAIN |
| [bind_external, async_bind_external](bind_external.md) | a SASL bind of EXTERNAL |
| [who_am_i, async_who_am_i](who_am_i.md) | the identity the session acts as |
| [start_tls, async_start_tls](start_tls.md) | the session on over TLS |
| [search, async_search](search.md) | the entries a filter matches |
| [add, async_add](add.md) | an entry made |
| [modify, async_modify](modify.md) | an entry's attributes changed |
| [remove, async_remove](remove.md) | an entry removed |
| [rename, async_rename](rename.md) | an entry renamed or moved |
| [compare, async_compare](compare.md) | whether an entry's attribute has a value |
| [extended, async_extended](extended.md) | an extended operation by its OID |
| [unbind, async_unbind](unbind.md) | the session ended |
| [close](close.md) | the connection closed without UnbindRequest |
| [is_tls](is_tls.md) | whether the session runs over TLS |
| [operator bool](operator_bool.md) | whether the handle holds a session |
| [operator==](operator_cmp.md) | whether two handles are the same session |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ldap.h"

using namespace sgcl;

int main() {
    net::ldap::client::options o;
    o.security = net::ldap::security::none;  // slapd on this machine, no TLS
    net::ldap::client c = net::ldap::client::connect("ldap://localhost:3890", o).value();
    c.bind("cn=admin,dc=example,dc=com", "secret").value();
    auto engineers = c.search("ou=people,dc=example,dc=com", "(title=Engineer)", {"cn", "mail"}).value();
    for (auto& e : engineers.entries) {
        println("{} <{}>", e.get("cn"), e.get("mail"));
    }
    c.unbind();
}
```

Output:

```text
Alice Liddell <alice@example.com>
Carol Danvers <carol@example.org>
```

## See also

- [options](../client-options.md)
- [ldap](../README.md)
