[sgcl](../../README.md) › [encoding](../README.md) › [yaml](README.md)

# sgcl::encoding::yaml::to_string

```cpp
string to_string() const;
```

The node as one document in block style, two spaces a level, every line ended: a string plain when it reads back
as the same string, a literal block (`|`, with its chomping and, for a first line of spaces, its indentation
indicator) for text of several lines, double quotes with escapes otherwise; `[]` and `{}` for empty collections; a
key that is a collection, or one past 1000 characters, after `? `; an application's tag before its node. A scalar
read keeps its text (`0o14`, `~`, `1e3`). A node shared by aliases is written at every place, so a text past 4 GiB
is `length_error`. Written without recursion, however deep.

## Parameters

None.

## Return value

The text.

## Complexity

Linear in the size of the node as written.

## Exceptions

`length_error` for a text past 4 GiB.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::yaml v = encoding::yaml::mapping({
        {"plain", "text"}, {"number-like", "123"}, {"lines", "first\nsecond\n"}, {"colon", "a: b"},
        {"read", encoding::yaml::parse("0x1F").value()}, {"list", encoding::yaml::sequence({1, encoding::yaml::sequence({})})}});
    print(v.to_string());
}
```

Output:

```text
plain: text
number-like: "123"
lines: |
  first
  second
colon: "a: b"
read: 0x1F
list:
  - 1
  - []
```

## See also

- [parse](parse.md)
- [sgcl::encoding::yaml](README.md)
