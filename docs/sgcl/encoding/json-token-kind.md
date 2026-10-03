[sgcl](../README.md) › [encoding](README.md) › [json](json/README.md) › [token](json-token/README.md)

# sgcl::encoding::json::token::kind

```cpp
#include "sgcl/encoding/json.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class json {
    public:
        class token {
        public:
            enum class kind : uint8_t {
                begin_object, end_object, begin_array, end_array, key, string, number, boolean, null
            };
        };
    };
}
```

`sgcl::encoding::json::token::kind` is what a [token](json-token/README.md) is, as its [type](json-token/type.md) says.
A key is a kind of its own, not a string: the reader has checked where it stands, so a loader tells a member's
key from a string value without counting anything itself. The commas and the colons are no tokens.

| Value | Description |
|---|---|
| `begin_object` | `{`, an object opened |
| `end_object` | `}`, the object open closed |
| `begin_array` | `[`, an array opened |
| `end_array` | `]`, the array open closed |
| `key` | the key of a member of an object, its characters decoded |
| `string` | a string value, its characters decoded |
| `number` | a number, its literal as the input wrote it |
| `boolean` | `true` or `false` |
| `null` | `null`; and a token made by its default constructor |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

const char* name_of(encoding::json::token::kind k) {
    using enum encoding::json::token::kind;
    switch (k) {
        case begin_object: return "begin_object";
        case end_object: return "end_object";
        case begin_array: return "begin_array";
        case end_array: return "end_array";
        case key: return "key";
        case string: return "string";
        case number: return "number";
        case boolean: return "boolean";
        case null: return "null";
    }
    return "";
}

int main() {
    encoding::json::reader r(string(R"({"a": ["b", 1, false, null]})"));
    while (auto t = r.next()) {
        print("{} ", name_of(t->type()));
    }
    println();
}
```

Output:

```text
begin_object key begin_array string number boolean null end_array end_object 
```

## See also

- [type](json-token/type.md): the kind of a token
- [json::reader::next](json-reader/next.md): what makes the tokens
- [sgcl::encoding::json::token](json-token/README.md)
