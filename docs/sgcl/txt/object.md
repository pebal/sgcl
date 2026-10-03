[sgcl](../README.md) › [txt](README.md)

# sgcl::txt::object

```cpp
#include "sgcl/txt/stencil.h"   // or "sgcl/txt.h"

namespace sgcl::txt {
    class object : public value;
}
```

A mapping of names to [values](value.md) written as data: `txt::object{{"name", "Ada"}, {"admin", true}}`. It is a
`value` and adds nothing to its data — no member, nothing virtual — so it is a value wherever one is wanted. A
mapping keeps the order it was written in, not the order of a hash ([ordered_map](../core/ordered_map.md)): a page
written twice running has to be the same page. A template reaches a name with a path, `{{ user.name }}`, and walks
the mapping with `range`, `.` a row of `key` and `value`; `format` writes it in braces, `{"name": "Ada"}`.

## Rules

- What a [value](value.md) holding a mapping is: it lives where a `tracked_ptr` may, and a copy shares the mapping —
  [set](object/set.md) on one is seen through every copy. So a mapping may be put inside itself; a field writes such
  a value as far as the ring closes: where a value would be written inside itself it is `...`, however many
  names lead back into the ring.
- A name written twice keeps its first place and its last value.
- [format](format.md) writes it as the value it is: `txt::format("{}", o)`.

## Member functions

| Function | Description |
|---|---|
| [(constructor)](object/object.md) | makes the mapping |
| [set](object/set.md) | sets the value of a name |

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    txt::value user = txt::object{{"name", "Ada"}, {"admin", true}, {"tags", {"maths", "engines"}}};
    txt::stencil t("{{ name }}{{ if admin }} (admin){{ end }}: {{ range tags }}#{{ . }} {{ end }}");
    println("{}", t.render(user));
    println("{}", user);
    return 0;
}
```

Output:

```text
Ada (admin): #maths #engines 
{"name": "Ada", "admin": true, "tags": ["maths", "engines"]}
```

## See also

- [list](list.md): a list
- [value](value.md)
