[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [memory_backend](README.md)

# sgcl::net::imap::memory_backend::create

```cpp
expected<void, io::error> create(const string& user, const string& name, const string& special_use) const;
```

Makes a mailbox, with a special use when one is given (`\Sent` ...); its parents are implied, not made.

## Parameters

| Parameter | Description |
|---|---|
| `user` | the user |
| `name` | the name, UTF-8, `/` the delimiter |
| `special_use` | a special use; empty for none |

## Return value

Nothing; or the error: `errc::already_exists`, `errc::cannot` for a name the store does not take (empty, an empty
level, `%` or `*`, control characters, past 1024 bytes).

## Complexity

Constant on average.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    net::imap::memory_backend mail;
    mail.add_user("alice", "secret");
    mail.create("alice", "Archive", net::imap::special_use::archive);
    println("{}", mail.create("alice", "Archive", "").error().code() == net::imap::errc::already_exists);
}
```

Output:

```text
true
```

## See also

- [remove](remove.md), [rename](rename.md)
- [mailboxes](mailboxes.md)
- [memory_backend](README.md)
