[sgcl](../../../README.md) › [net](../../README.md) › [dns_sd](../README.md) › [txt_record](README.md)

# sgcl::net::dns_sd::txt_record::get

```cpp
optional<string> get(const string& key) const noexcept;
```

Returns the value of a key, found without regard to case: what follows its first `=`. A key set alone, an attribute
with no value (§6.4), has none, as a key not there has none: [contains](contains.md) tells the two apart.

## Parameters

| Parameter | Description |
|---|---|
| `key` | the key |

## Return value

The value, empty for `"key="`; `nullopt` for a key alone and for a key not there.

## Complexity

Linear in the keys.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net.h"

using namespace sgcl;

int main() {
    net::dns_sd::txt_record txt = {{"path", "/a=b"}, {"empty", ""}};
    txt.set("secure");
    println("{}", txt.get("Path").value());
    println("{}", txt.get("empty").value().empty());
    println("{} {}", txt.get("secure").has_value(), txt.contains("secure"));
    println("{}", txt.get("missing").has_value());
}
```

Output:

```text
/a=b
true
false true
false
```

## See also

- [contains](contains.md): whether a key is there
- [set](set.md): the other way
- [sgcl::net::dns_sd::txt_record](README.md)
