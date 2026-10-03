[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml/README.md) › [token](README.md)

# sgcl::encoding::xml::token::is_start

```cpp
bool is_start(const string& name) const noexcept;
```

Checks whether the token is the start of an element of this name, matched as written (`dc:title`) or by namespace
and local name (`{http://purl.org/dc/elements/1.1/}title`): the question a loop over a reader's
[peek](../xml-reader/peek.md) asks before it [reads](../xml-reader/read.md) or steps over an element.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the element, as written or `{namespace}local` |

## Return value

`true` when the token is a start of that name, `false` otherwise.

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
    encoding::xml::reader r("<feed xmlns='urn:a'><entry/><x:entry xmlns:x='u'/><entry/></feed>");
    int entries = 0;
    while (auto t = r.next()) {
        if (t->is_start("{urn:a}entry")) {
            ++entries;
        }
    }
    println(entries);
}
```

Output:

```text
2
```

## See also

- [is_end](is_end.md): the end of an element of a name
- [reader::peek](../xml-reader/peek.md)
- [sgcl::encoding::xml::token](README.md)
