[sgcl](../README.md) › [encoding](README.md) › [json](json.md)

# sgcl::encoding::json::member

```cpp
#include "sgcl/encoding/json.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class json {
    public:
        struct member;
    };

    struct json::member {
        string key;
        json value;
    };
}
```

`sgcl::encoding::json::member` is a member of a JSON object: its key and its value. An aggregate of two fields, it
is what [members](json/members.md) gives a slice of, taken apart as `[key, value]`, and what
[object](json/object.md) takes a list of, `{"name", "Ala"}`.

## Rules

- A member holds a [string](../core/string.md) and a [json](json.md), so it lives where they may: on a stack or
  inside a managed object.
- The members of a value never change: a member of [members](json/members.md) is `const`.

## Member objects

| Object | Description |
|---|---|
| `key` | the key, UTF-8; empty when default-constructed |
| `value` | the value; null when default-constructed |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json::member port = {"port", 443};
    encoding::json config = encoding::json::object({{"host", "example.com"}, port});
    for (auto& [key, value] : config.members()) {
        println("{} = {}", key, value.to_string());
    }
}
```

Output:

```text
host = "example.com"
port = 443
```

## See also

- [members](json/members.md): the members of an object
- [object](json/object.md): an object of members
- [sgcl::encoding::json](json.md)
