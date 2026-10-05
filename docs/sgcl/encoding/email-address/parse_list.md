[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [address](README.md)

# sgcl::encoding::email::address::parse_list

```cpp
static expected<vector<address>, error> parse_list(const string& text);
```

An address list read from text: mailboxes and groups (`Team: a@x, b@y;`) separated by commas, a group's members in
its place, an empty group and an empty element (`,,`, RFC 5322 §4.4) nothing.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the list |

## Return value

The addresses, in order; or the [error](../error/README.md) `errc::syntax` where the text stops being a list.

## Complexity

Linear in the size of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    string text = "a@x.example,, Undisclosed:;, B <b@x.example>";
    auto list = encoding::email::address::parse_list(text);
    println("{}", list->size());
    println("{}", encoding::email::address::parse_list("Team: a@x.example").error().message());
}
```

Output:

```text
2
offset 17: invalid address
```

## See also

- [parse](parse.md)
- [address](README.md)
