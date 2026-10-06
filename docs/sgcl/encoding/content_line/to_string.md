[sgcl](../../README.md) › [encoding](../README.md) › [content_line](README.md)

# sgcl::encoding::content_line::to_string

```cpp
string to_string() const;
```

The line as a file holds it, by the [rules](README.md#rules): folded at 75 octets, each part ended by CRLF; a
parameter value with `:`, `;` or `,` in quotes, a line break, `^` and `"` written as RFC 6868's escapes; a control
character of a value (none [parse](parse.md) gives) written as a space.

## Parameters

None.

## Return value

The text.

## Complexity

Linear in the size of the line.

## Exceptions

`invalid_argument` for a name, a parameter's name or a group that is none.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::content_line line("X-NOTE", {{"LABEL", {"a:b", "two\nlines"}}}, string(std::string(90, 'x')));
    print(line.to_string());
}
```

Output:

```text
X-NOTE;LABEL="a:b",two^nlines:xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
 xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
```

## See also

- [parse](parse.md)
- [sgcl::encoding::content_line](README.md)
