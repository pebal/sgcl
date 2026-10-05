[sgcl](../../../README.md) › [net](../../README.md) › [dns_sd](../README.md)

# sgcl::net::dns_sd::txt_record

```cpp
#include "sgcl/net/mdns.h"   // or "sgcl/net.h"

namespace sgcl::net::dns_sd {
    class txt_record;
}
```

**Requires [rooted](../../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::net::dns_sd::txt_record` is the TXT record of a DNS-SD service (RFC 6763 §6): its settings as keys, each with
a value (`"path=/api"`) or alone (`"secure"`, an attribute present, §6.4), in the order they were set, the way a
[service](../service.md) carries them and [resolve](../resolve.md) gives them back. A key is printable ASCII without
`=` and is compared without regard to case; a value is any bytes, `=` among them; an entry, the key with its `=` and
value, is at most 255 bytes, a string of the record. Python's zeroconf has a dictionary of bytes, Apple's dns-sd a
TXTRecordRef; here a small value type, a copy of which copies the entries.

## Rules

- A key set again keeps its place and takes the new value; the record never holds a key twice.
- [set](set.md) of a key that is empty, holds `=` or a byte outside `0x20`–`0x7E`, or of an entry past 255 bytes
  throws `invalid_argument` and changes nothing.
- A record read off the wire keeps the first of a key's entries and drops the others, the empty strings and those
  that start with `=` (§6.4); an empty record is sent as one empty string (§6.1).

## Member functions

| Function | Description |
|---|---|
| [(constructor)](txt_record.md) | an empty record, or one of keys and values |

#### Lookup

| Function | Description |
|---|---|
| [contains](contains.md) | whether a key is there |
| [get](get.md) | a key's value |
| [keys](keys.md) | the keys in their order |
| [entries](entries.md) | the entries as the record carries them |

#### Capacity

| Function | Description |
|---|---|
| [empty](empty.md) | whether there is no key |
| [size](size.md) | the number of keys |

#### Modifiers

| Function | Description |
|---|---|
| [set](set.md) | a key with its value, or alone |
| [remove](remove.md) | a key taken out |

## Non-member functions

| Function | Description |
|---|---|
| [operator==, operator!=](operator_cmp.md) | whether two records hold the same entries in the same order |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::dns_sd::txt_record txt = {{"path", "/api"}, {"version", "2"}};
    txt.set("secure");
    txt.set("Version", "3");  // the same key
    for (auto& e : txt.entries()) {
        println("{}", e);
    }
    println("{} {}", txt.get("PATH").value(), txt.contains("secure"));
}
```

Output:

```text
path=/api
Version=3
secure
/api true
```

## See also

- [service](../service.md): where it is carried
- [resolve](../resolve.md): a record read off the link
- [sgcl::net::dns_sd](../README.md)
