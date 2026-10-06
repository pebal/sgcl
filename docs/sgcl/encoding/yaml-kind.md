[sgcl](../README.md) › [encoding](README.md) › [yaml](yaml/README.md)

# sgcl::encoding::yaml::kind

```cpp
#include "sgcl/encoding/yaml.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class yaml {
    public:
        enum class kind : uint8_t {
            null,
            boolean,
            integer,
            floating,
            string,
            sequence,
            mapping
        };
    };
}
```

`sgcl::encoding::yaml::kind` is the kind of a [yaml](yaml/README.md) node, as [type](yaml/type.md) gives it: the kinds
of YAML 1.2's core schema (§10.3) and the two collections.

| Value | Description |
|---|---|
| `null` | `null`, `~`, nothing; what a lookup that found nothing gives |
| `boolean` | `true`, `false` in three cases |
| `integer` | decimal, `0o` octal, `0x` hexadecimal, of any size |
| `floating` | a float, `.inf` and `.nan` among them |
| `string` | a quoted scalar, or a plain one the schema reads as nothing else |
| `sequence` | a sequence |
| `mapping` | a mapping of keys of any kind |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto v = encoding::yaml::parse("[0x1F, '0x1F']").value();
    println(v[0].type() == encoding::yaml::kind::integer);
    println(v[1].type() == encoding::yaml::kind::string);
}
```

Output:

```text
true
true
```

## See also

- [type](yaml/type.md)
- [sgcl::encoding::yaml](yaml/README.md)
