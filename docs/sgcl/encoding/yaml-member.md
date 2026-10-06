[sgcl](../README.md) › [encoding](README.md) › [yaml](yaml/README.md)

# sgcl::encoding::yaml::member

```cpp
#include "sgcl/encoding/yaml.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class yaml {
    public:
        struct member {
            yaml key;
            yaml value;
        };
    };
}
```

**Requires [rooted](../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::yaml::member` is a member of a mapping: a key of any kind and its value, what
[mapping](yaml/mapping.md) takes and [members](yaml/members.md) gives; it works with `[key, value]`.

## Member objects

| Object | Description |
|---|---|
| `key` | the key |
| `value` | its value |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::yaml::member m{"replicas", 3};
    print(encoding::yaml::mapping({m}).to_string());
}
```

Output:

```text
replicas: 3
```

## See also

- [mapping](yaml/mapping.md), [members](yaml/members.md)
- [sgcl::encoding::yaml](yaml/README.md)
