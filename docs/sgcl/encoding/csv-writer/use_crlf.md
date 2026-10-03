[sgcl](../../README.md) › [encoding](../README.md) › [csv](../csv/README.md) › [writer](README.md)

# sgcl::encoding::csv::writer::use_crlf

```cpp
writer& use_crlf() noexcept;
```

Ends every record written after it with `"\r\n"`, which RFC 4180 asks for, in place of `'\n'`; a `'\n'` inside a
quoted field is written `"\r\n"` too, and a `'\r'` there is dropped. Go's `UseCRLF`. It returns the writer, so that
it chains: `w.use_crlf().write({"a", "b"})`.

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
    io::buffer out;
    encoding::csv::writer w(out);
    w.use_crlf().write({"a", "two\nlines"}).write({"b", "c"});
    w.flush().value();
    for (char c : out.text()) {
        print("{}", c == '\r' ? string("\\r") : c == '\n' ? string("\\n") : string(1, c));
    }
    println();
}
```

Output:

```text
a,"two\r\nlines"\r\nb,c\r\n
```

## See also

- [write](write.md): a record
- [sgcl::encoding::csv::writer](README.md)
