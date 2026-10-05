[sgcl](../../../README.md) › [net](../../README.md) › [imap](../README.md) › [sequence_set](README.md)

# sgcl::net::imap::sequence_set::parse

```cpp
static optional<sequence_set> parse(const string& text) noexcept;       // (1)
static optional<sequence_set> parse(std::string_view text) noexcept;    // (2)
```

Reads a sequence set: numbers (1 to 2³² − 1) or `*`, ranges with `:`, separated by `,`; or `$`.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text |

## Return value

The set; `nullopt` for a text that is none (empty, a 0, a number past 32 bits, a trailing comma).

## Complexity

Linear in the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/io.h"
#include "sgcl/net/imap.h"

using namespace sgcl;

int main() {
    println("{}", net::imap::sequence_set::parse("1:3,*").has_value());
    println("{}", net::imap::sequence_set::parse("0").has_value());
    println("{}", net::imap::sequence_set::parse("4,").has_value());
}
```

Output:

```text
true
false
false
```

## See also

- [(constructor)](sequence_set.md)
- [sequence_set](README.md)
