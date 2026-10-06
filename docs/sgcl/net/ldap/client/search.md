[sgcl](../../../README.md) › [net](../../README.md) › [ldap](../README.md) › [client](README.md)

# sgcl::net::ldap::client::search, async_search

```cpp
expected<search_result, io::error> search(const search_request& r) const;                                       // (1)
async::task<expected<search_result, io::error>> async_search(search_request r) const noexcept;                  // (2)
expected<search_result, io::error> search(const string& base, const string& filter,                             // (3)
                                          const vector<string>& attributes = {}) const;
async::task<expected<search_result, io::error>> async_search(string base, string filter,                        // (4)
                                                             vector<string> attributes = {}) const noexcept;
```

A search (RFC 4511 §4.5): the entries under the base that the filter matches, with the attributes asked for, and
the references to other servers met on the way. The filter is RFC 4515's text, read into BER before anything
is sent.

- (1–2) As the [request](../search_request.md) says: the scope, the limits, aliases, types only, paging. With a
  page size, pages are asked for (RFC 2696) until the server says the last came, every entry in one result.
- (3–4) The subtree of the base, in one line.

A size or time limit reached ends the search with the entries found until then, `truncated` set.

`search` waits on the calling thread; a task awaits `async_search`.

## Parameters

| Parameter | Description |
|---|---|
| `r` | the whole request |
| `base` | the DN the search starts at |
| `filter` | RFC 4515's text: "(&(objectClass=person)(cn=J*))" |
| `attributes` | the attributes asked for; empty: all user attributes |


## Return value

The entries and references; `errc::invalid_filter` for a filter that does not read, `errc::no_such_object` for a
base that is not there, the server's other results as their codes.

## Complexity

A round trip per page; linear in the entries.

## Exceptions

- (1), (3) `std::system_error` when the wait starts the scheduler and a worker's thread cannot be started.
- (2), (4) None.

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
    net::ldap::search_request q;
    q.base = "dc=example,dc=com";
    q.filter = "(&(objectClass=inetOrgPerson)(mail=*@example.com))";
    q.attributes = {"uid", "mail"};
    net::ldap::search_result r = c.search(q).value();
    for (auto& e : r.entries) {
        println("{} {}", e.get("uid"), e.get("mail"));
    }
}
```

Output:

```text
alice alice@example.com
bob bob@example.com
```

```cpp
#include "sgcl/io.h"
#include "sgcl/net/ldap.h"

using namespace sgcl;

int main() {
    net::ldap::client::options o;
    o.security = net::ldap::security::none;  // slapd on this machine, no TLS
    net::ldap::client c = net::ldap::client::connect("ldap://localhost:3890", o).value();
    c.bind("cn=admin,dc=example,dc=com", "secret").value();
    auto group = c.search("ou=groups,dc=example,dc=com", "(cn=engineers)", {"member"}).value();
    for (auto& m : group.entries[0].get_all("member")) {
        println("{}", m);
    }
    auto bad = c.search("dc=example,dc=com", "(cn=unclosed");
    println("{}", bad.error().code() == net::ldap::errc::invalid_filter);
}
```

Output:

```text
uid=alice,ou=people,dc=example,dc=com
uid=carol,ou=people,dc=example,dc=com
true
```

## See also

- [search_request](../search_request.md)
- [search_result](../search_result.md)
- [filter_escape](../filter_escape.md)
- [client](README.md)
