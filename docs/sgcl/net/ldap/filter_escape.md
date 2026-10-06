[sgcl](../../README.md) › [net](../README.md) › [ldap](README.md)

# sgcl::net::ldap::filter_escape

```cpp
string filter_escape(const string& value);
```

A value escaped for a filter's text (RFC 4515 §3): `*`, `(`, `)`, `\` and NUL as `\XX`, so that the text of a user
matches as itself and never as a filter of its own ("a*b" is `a\2ab`).

## Parameters

| Parameter | Description |
|---|---|
| `value` | the text, as the user gave it |


## Return value

The escaped text.

## Complexity

Linear in the value.

## Exceptions

None.

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
    println("{}", net::ldap::filter_escape("*)(uid=*"));
    string filter = string::concat("(cn=", net::ldap::filter_escape("Alice*"), ")");
    println("{}", c.search("ou=people,dc=example,dc=com", filter)->entries.size());
}
```

Output:

```text
\2a\29\28uid=\2a
0
```

## See also

- [search](client/search.md)
