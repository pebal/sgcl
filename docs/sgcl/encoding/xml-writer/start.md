[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md) › [writer](../xml-writer.md)

# sgcl::encoding::xml::writer::start

```cpp
writer& start(const string& name) noexcept;
```

Writes the start tag of the element `name`, inside the element open, and opens it: the tag stays open for
[attributes](attribute.md) until something comes after them, and [end](end.md) closes the element. A name that is
not a qualified name, or is of the prefix `xmlns`, is a mistake (`errc::syntax`), kept and given by [flush](flush.md). The prefix is the
program's to declare, with an `xmlns:p` attribute.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the element, with its prefix |

## Return value

`*this`.

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
    encoding::xml::writer w(io::stdout);
    w.start("x:doc").attribute("xmlns:x", "urn:x").start("x:p").text("hi").end().end();
    w.flush().value();
    println();

    encoding::xml::writer bad(io::stdout);
    bad.start("two words");
    println(bad.last_error()->message());
}
```

Output:

```text
<x:doc xmlns:x="urn:x"><x:p>hi</x:p></x:doc>
'two words' is not a qualified name
```

## See also

- [end](end.md): the end of the element started last
- [attribute](attribute.md): an attribute of the element just started
- [sgcl::encoding::xml::writer](../xml-writer.md)
