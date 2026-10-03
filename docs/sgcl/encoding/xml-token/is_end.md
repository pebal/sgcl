[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml/README.md) › [token](README.md)

# sgcl::encoding::xml::token::is_end

```cpp
bool is_end(const string& name) const noexcept;
```

Checks whether the token is the end of an element of this name, matched as written or by namespace and local
name, `{namespace}local`. `<a/>` gives an end as well as a start.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the element, as written or `{namespace}local` |

## Return value

`true` when the token is an end of that name, `false` otherwise.

## Complexity

Linear in the length of `name`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::reader r("<list><item>a</item><item/></list>");
    while (auto t = r.next()) {
        if (t->is_end("item")) {
            println("an item ends at depth {}", r.depth());
        }
    }
}
```

Output:

```text
an item ends at depth 1
an item ends at depth 1
```

## See also

- [is_start](is_start.md): the start of an element of a name
- [sgcl::encoding::xml::token](README.md)
