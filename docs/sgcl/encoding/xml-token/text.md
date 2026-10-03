[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml/README.md) › [token](README.md)

# sgcl::encoding::xml::token::text

```cpp
const string& text() const noexcept;
```

The text of a text token, its references replaced and its line endings `"\n"`; of a comment, the comment; of an
instruction, its data (the XML declaration's is `version="1.0" encoding="UTF-8"`); of the DOCTYPE declaration, all
of it after its keyword (`html PUBLIC ...`), uninterpreted. Empty for a start and an end.

## Parameters

None.

## Return value

The text, or an empty string.

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::reader r(
        "<?xml version='1.0'?><!DOCTYPE note SYSTEM 'note.dtd'><note>a &lt; b<!-- why --></note>");
    while (auto t = r.next()) {
        println("[{}]", t->text());
    }
}
```

Output:

```text
[version='1.0']
[note SYSTEM 'note.dtd']
[]
[a < b]
[ why ]
[]
```

## See also

- [type](type.md): what the token is
- [sgcl::encoding::xml::token](README.md)
