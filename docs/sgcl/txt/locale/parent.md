[sgcl](../../README.md) › [txt](../README.md) › [locale](README.md)

# sgcl::txt::locale::parent

```cpp
locale parent() const noexcept;
```

Returns the locale CLDR's data of this one inherits from: the one `parentLocales` names (`es-MX` is `es-419`,
`en-GB` `en-001`, `pt-AO` `pt-PT`, `nb` `no`), else the locale with its last subtag cut (`es-419` is `es`,
`sr-Latn-BA` `sr-Latn`), and the root locale for a language alone and for a script the language is not usually
written in (`sr-Latn`). The root locale's parent is the root locale.

## Parameters

None.

## Return value

The parent locale.

## Complexity

Logarithmic in the size of CLDR's table.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/txt.h"

using namespace sgcl;

int main() {
    for (txt::locale l("es-MX"); l != txt::locale(); l = l.parent()) {
        println("{}", l.to_string());
    }
}
```

Output:

```text
es-MX
es-419
es
```

## See also

- [maximize](maximize.md)
- [sgcl::txt::locale](README.md)
