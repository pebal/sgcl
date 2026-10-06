[sgcl](../../README.md) › [encoding](../README.md) › [vcard](README.md)

# sgcl::encoding::vcard::vcard

```cpp
vcard() noexcept;
```

A card of `VERSION:4.0` alone; [add](add.md) gives it its FN and the rest. The copy and the move are the implicit
ones and copy the handle.

## Parameters

None.

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
    encoding::vcard card = encoding::vcard().add(encoding::content_line::text("FN", "Jan Kowalski"))
                                            .add(encoding::content_line("EMAIL", {{"TYPE", {"work"}}}, "jan@example.com"));
    print(card.to_string());
}
```

Output:

```text
BEGIN:VCARD
VERSION:4.0
FN:Jan Kowalski
EMAIL;TYPE=work:jan@example.com
END:VCARD
```

## See also

- [add](add.md)
- [sgcl::encoding::vcard](README.md)
