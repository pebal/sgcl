[sgcl](../README.md) › [encoding](README.md) › [json](json.md)

# sgcl::encoding::json::style

```cpp
#include "sgcl/encoding/json.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class json {
    public:
        struct style {
            uint8_t indent = 0;
            bool escape_html = false;
            bool sort_keys = true;
        };

        static const style compact;
        static const style pretty;
    };
}
```

`sgcl::encoding::json::style` is how a value is written: by [to_string](json/to_string.md),
[stringify](json/stringify.md) and the [writer](json-writer.md). Two are constants of the class:
[compact](json.md#member-objects), the default, with no space at all, and [pretty](json.md#member-objects), an
indent of 2, the text of Go's `MarshalIndent(v, "", "  ")`. A plain struct: set the fields that differ and pass
it.

## Rules

- `style` is plain data and holds no pointer: it lives anywhere, and a constant of it may be global.
- With an indent, each element and member is on a line of its own, a space after the colon, and an empty array or
  object stays `[]` or `{}`.

## Member objects

| Object | Description |
|---|---|
| `indent` | the spaces of one level; 0 for the compact text, on one line with no space at all; 0 |
| `escape_html` | `<`, `>` and `&` in a string written as `<`, `>` and `&`, for a text put inside HTML (Go's `SetEscapeHTML`, off by default as in v2); `false` |
| `sort_keys` | the keys of a hash map and the elements of a hash set of a program's value ([stringify](json/stringify.md)) written sorted by their text, as Go sorts the keys of a map, so that the text does not depend on the hash; `false` keeps their own order. A json's members are always written in their order; `true` |

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    map<string, string> links = {{"home", "<a href=\"/\">"}, {"about", "&about"}};

    encoding::json::style s;
    s.indent = 4;
    s.escape_html = true;
    println(encoding::json::stringify(links, s).value());
    println(encoding::json::stringify(links, encoding::json::compact).value());
}
```

Output:

```text
{
    "about": "\u0026about",
    "home": "\u003ca href=\"/\"\u003e"
}
{"about":"&about","home":"<a href=\"/\">"}
```

## See also

- [to_string](json/to_string.md), [stringify](json/stringify.md): the text in a style
- [options](json-options.md): what a parse accepts
- [sgcl::encoding::json](json.md)
