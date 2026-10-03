[sgcl](../README.md) › [encoding](README.md) › [json](json.md)

# sgcl::encoding::json::kind

```cpp
#include "sgcl/encoding/json.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class json {
    public:
        enum class kind : uint8_t {
            null,
            boolean,
            number,
            string,
            array,
            object
        };
    };
}
```

`sgcl::encoding::json::kind` is the kind of a [json](json.md) value, the six of JSON, as
[type](json/type.md) gives it: what a type switch over Go's `any` tells apart. A number is one kind however it is
held — an integer, a double or a literal kept as its text; [is_integer](json/is_integer.md) asks which.

| Value | Description |
|---|---|
| `null` | `null`; a value made by the default constructor, and what a lookup that found nothing gives |
| `boolean` | `true` or `false` |
| `number` | a number |
| `string` | a string |
| `array` | an array, empty or not |
| `object` | an object, empty or not |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::json doc = encoding::json::parse(R"({"id": 7, "tags": ["a"], "boss": null})");
    int numbers = 0, arrays = 0, nulls = 0;
    for (auto& [key, value] : doc.members()) {
        switch (value.type()) {
            case encoding::json::kind::number: ++numbers; break;
            case encoding::json::kind::array: ++arrays; break;
            case encoding::json::kind::null: ++nulls; break;
            default: break;
        }
    }
    println("{} {} {}", numbers, arrays, nulls);
}
```

Output:

```text
1 1 1
```

## See also

- [type](json/type.md): the kind of a value
- [sgcl::encoding::json](json.md)
