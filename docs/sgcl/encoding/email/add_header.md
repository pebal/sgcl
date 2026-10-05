[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::add_header

```cpp
email& add_header(const string& name, const string& value);
```

A field of `name` and the text `value` after the others, whatever fields of the name there are. Encoded when
written, as [set_header](set_header.md)'s.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the field's name |
| `value` | its text |

## Return value

`*this`.

## Complexity

Constant.

## Exceptions

`invalid_argument` when `name` is not a field name.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    encoding::email m;
    m.add_header("Keywords", "one").add_header("Keywords", "two");
    println("{}", m.header_all("keywords").size());
}
```

Output:

```text
2
```

## See also

- [set_header](set_header.md)
- [header_all](header_all.md)
- [email](README.md)
