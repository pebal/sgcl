[sgcl](../../README.md) › [encoding](../README.md) › [icalendar](README.md)

# sgcl::encoding::icalendar::to_string

```cpp
string to_string() const;
```

The component as a file holds it: `BEGIN:` and its name, its properties, the components inside it, `END:` and its
name, each line folded at 75 octets and ended by CRLF. Written without recursion, however deep.

## Parameters

None.

## Return value

The text.

## Complexity

Linear in the size of the component.

## Exceptions

`invalid_argument` for a component's or a property's name that is none.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto todo = encoding::icalendar("VTODO").add(encoding::content_line("UID", "1")).add(encoding::content_line("DUE", "20261009T170000Z"));
    print(todo.to_string());
}
```

Output:

```text
BEGIN:VTODO
UID:1
DUE:20261009T170000Z
END:VTODO
```

## See also

- [parse](parse.md)
- [save](save.md)
- [sgcl::encoding::icalendar](README.md)
