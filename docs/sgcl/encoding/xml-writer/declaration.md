[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml/README.md) › [writer](README.md)

# sgcl::encoding::xml::writer::declaration

```cpp
writer& declaration() noexcept;
```

Writes the XML declaration, `<?xml version="1.0" encoding="UTF-8"?>`, which only the first thing written may be.
After anything else, the declaration is a mistake (`errc::syntax`), kept and given by [flush](flush.md). A writer
made with [style](../xml-style.md)`::declaration` has written it already.

## Parameters

None.

## Return value

`*this`.

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
    encoding::xml::writer w(io::stdout);
    w.declaration().start("a").end();
    w.flush().value();
    println();

    encoding::xml::writer late(io::stdout);
    late.start("a").declaration();
    println(late.last_error()->message());
}
```

Output:

```text
<?xml version="1.0" encoding="UTF-8"?><a/>
the XML declaration after something was written
```

## See also

- [instruction](instruction.md): a processing instruction, whose target is never `xml`
- [sgcl::encoding::xml::writer](README.md)
