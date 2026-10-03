[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml/README.md) › [token](README.md)

# sgcl::encoding::xml::token::attribute

```cpp
optional<string> attribute(const string& name) const noexcept;                  // (1)
string attribute(const string& name, const string& fallback) const noexcept;    // (2)
```

The value of an attribute of a start, as [xml::attribute](../xml/attribute.md) of a node gives it.

1. The value, or `nullopt` when the start has no such attribute, or the token is not a start.
2. The same with a value for when there is none.

The name is matched as written, prefix included (`id`, `xlink:href`), or by namespace and local name when it is
written `{namespace}local`; an attribute with no prefix is `{}id`.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name, as written or `{namespace}local` |
| `fallback` | the value when there is no such attribute |

## Return value

1. The value of the attribute, or `nullopt`.
2. The value of the attribute, or `fallback`.

## Complexity

Linear in the number of attributes of the start.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::reader r("<links xmlns:xl='urn:xl'><a xl:href='#1'/><a/></links>");
    while (auto t = r.next()) {
        if (t->is_start("a")) {
            println(t->attribute("{urn:xl}href", "-"));
        }
    }
}
```

Output:

```text
#1
-
```

## See also

- [attributes](attributes.md): every attribute, in order
- [sgcl::encoding::xml::token](README.md)
