[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md) › [writer](../xml-writer.md)

# sgcl::encoding::xml::writer::attribute

```cpp
writer& attribute(const string& name, const string& value) noexcept;
```

Writes an attribute into the start tag just written, its value in `"` and escaped: `&lt;`, `&amp;`, `&gt;`,
`&quot;`, and a tab, a line feed and a carriage return as character references, which a reader would otherwise
make spaces; a character XML cannot hold is written as U+FFFD. Each of these is a mistake, kept and given by
[flush](flush.md): an attribute with no start tag open — after the content of the element began, or before any
start — a name that is not a qualified name and a namespace declaration Namespaces in XML forbids, as
[xml::set](../xml/set.md) says (`errc::syntax`), the same name twice in one tag
(`errc::duplicate_key`).

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the attribute, with its prefix |
| `value` | its value, as it is |

## Return value

`*this`.

## Complexity

Linear in the length of `name` and `value`, and in the number of attributes of the tag up to 16; constant on
average past that.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::writer w(io::stdout);
    w.start("a").attribute("href", "?q=\"x\"&n=1").attribute("title", "line 1\nline 2").end();
    w.flush().value();
    println();

    encoding::xml::writer twice(io::stdout);
    twice.start("a").attribute("id", "1").attribute("id", "2");
    println(twice.last_error()->message());
}
```

Output:

```text
<a href="?q=&quot;x&quot;&amp;n=1" title="line 1&#xA;line 2"/>
the attribute id twice in one tag
```

## See also

- [start](start.md): the start tag the attributes belong to
- [sgcl::encoding::xml::writer](../xml-writer.md)
