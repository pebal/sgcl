[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::subject

```cpp
string subject() const;
```

The Subject field as [header](header.md) gives it: unfolded, its encoded words decoded.

## Parameters

None.

## Return value

The subject; `""` when there is none.

## Complexity

Linear in the size of the field.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    string head = "Subject: =?utf-8?b?WmHFvMOzxYLEhw==?= =?utf-8?q?_g=C4=99=C5=9Bl=C4=85?=\r\n\r\n";
    auto m = encoding::email::parse(head).value();
    println("{}", m.subject());
}
```

Output:

```text
Zażółć gęślą
```

## See also

- [set_subject](set_subject.md)
- [email](README.md)
