[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml/README.md) › [writer](README.md)

# sgcl::encoding::xml::writer::comment

```cpp
writer& comment(const string& t) noexcept;
```

Writes a comment, `<!--t-->`, a character XML cannot hold as U+FFFD. A text holding `--` or ending with `-` is a
mistake (`errc::syntax`), kept and given by [flush](flush.md). With an indentation, a comment stands on a line of
its own, as an element does.

## Parameters

| Parameter | Description |
|---|---|
| `t` | the text of the comment |

## Return value

`*this`.

## Complexity

Linear in the length of `t`.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::writer w(io::stdout, encoding::xml::pretty);
    w.start("config").comment(" defaults ").start("port").text("80").end().end();
    w.flush().value();
    println();

    encoding::xml::writer bad(io::stdout);
    bad.comment("a--b");
    println(bad.last_error()->message());
}
```

Output:

```text
<config>
  <!-- defaults -->
  <port>80</port>
</config>
a comment cannot hold "--" or end with '-'
```

## See also

- [instruction](instruction.md): a processing instruction
- [sgcl::encoding::xml::writer](README.md)
