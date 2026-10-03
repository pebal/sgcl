[sgcl](../../README.md) › [encoding](../README.md) › [error](../error.md)

# sgcl::encoding::error::locate

```cpp
error& locate(const string& text) noexcept;
```

Sets the line and the column of [offset()](offset.md) in `text`, the input the error was found in: the lines are
counted from its start to the offset, a line ending at `'\n'` (a `'\r'` before it is the last character of its
line), and the column in code points; an offset past the end of the text is its end. It is the after-the-fact count
the formats of the module make on the path of an error alone, so that a reading that succeeds never counts a line
ending; a program that reads an input of its own calls it when its reading fails.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the input the offset is in |

## Return value

`*this`, so that it chains: `error(code, at, detail).locate(text)`.

## Complexity

Linear in the offset.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = "[1,\n2,,3]";
    encoding::error e(encoding::errc::syntax, 5, "unexpected ','");
    println("{}", e.message());
    e.locate(text);
    println("{} {}", e.line(), e.message());
    println("{}", encoding::error(encoding::errc::syntax, 6).locate("ż\nżółw").message());
}
```

Output:

```text
offset 5: unexpected ','
2 2:2: unexpected ','
2:3: syntax error
```

## See also

- [set_position](set_position.md): the line and the column given as they are
- [line](line.md), [column](column.md): what it sets
- [sgcl::encoding::error](../error.md)
