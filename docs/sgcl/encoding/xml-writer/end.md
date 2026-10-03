[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md) › [writer](../xml-writer.md)

# sgcl::encoding::xml::writer::end

```cpp
writer& end() noexcept;
```

Ends the element started last: `/>` when it has no content (`<empty/>`), its end tag otherwise. With no element
open it is a mistake (`errc::mismatched_tag`), kept and given by [flush](flush.md).

## Parameters

None.

## Return value

`*this`.

## Complexity

Linear in the length of the element's name.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::writer w(io::stdout);
    w.start("row").start("cell").end().start("cell").text("1").end().end();
    w.flush().value();
    println();

    w.end();
    println(w.last_error()->message());
}
```

Output:

```text
<row><cell/><cell>1</cell></row>
an end with no element open
```

## See also

- [start](start.md): the start of an element
- [depth](depth.md): the elements open
- [sgcl::encoding::xml::writer](../xml-writer.md)
