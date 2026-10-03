[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml/README.md) › [writer](README.md)

# sgcl::encoding::xml::writer::last_error

```cpp
const optional<error>& last_error() const noexcept;
```

The first mistake the program made with the writer, whole, or `nullopt` while it has made none: its
[errc](../errc.md) code, a message, and for a value of a program's type the path inside it. It came from no input
text, so it has no place: its offset, line and column are 0, and its message is the path and the words. Nothing is
written after a mistake; [flush](flush.md) gives it as an `io::error`.

## Parameters

None.

## Return value

The mistake, or `nullopt`.

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
    w.start("a").end().end().start("b");
    if (auto& e = w.last_error()) {
        println(e->code() == encoding::errc::mismatched_tag);
        println(e->message());
    }
}
```

Output:

```text
true
an end with no element open
```

## See also

- [flush](flush.md): the mistake as an `io::error`
- [error](../error/README.md), [errc](../errc.md)
- [sgcl::encoding::xml::writer](README.md)
