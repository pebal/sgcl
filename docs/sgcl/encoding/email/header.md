[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::header

```cpp
string header(const string& name) const;
```

The first value of the field `name` (its case does not matter), as text: unfolded, trimmed, its encoded words
decoded. A field set by the program comes back as it was set.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the field's name |

## Return value

The value, or `""` when there is no such field ([has_header](has_header.md) tells the two apart).

## Complexity

Linear in the number of fields and the size of the value.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    string head = "Subject: =?ISO-8859-1?Q?Caf=E9?=\r\n crème\r\nX-Mailer: test\r\n\r\n";
    auto m = encoding::email::parse(head).value();
    println("{}", m.header("subject"));
    println("{}", m.header("X-MAILER"));
    println("'{}'", m.header("X-None"));
}
```

Output:

```text
Café crème
test
''
```

## See also

- [header_all](header_all.md)
- [set_header](set_header.md)
- [email](README.md)
