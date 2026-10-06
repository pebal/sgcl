[sgcl](../../README.md) › [encoding](../README.md) › [vcard](README.md)

# sgcl::encoding::vcard::parse_all

```cpp
static expected<vector<vcard>, error> parse_all(const string& text) noexcept;                      // (1)
static expected<vector<vcard>, error> parse_all(const string& text, const options& o) noexcept;    // (2)
```

Every card of the text, in order: an address book, as phones and mail programs export one.

1. With the default [options](../vcard-options.md).
2. With the options given.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the text |
| `o` | what is accepted |

## Return value

The cards, or the [error](../error/README.md) of the first that is not one, as [parse](parse.md) gives it.

## Complexity

Linear in the length of the text.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto book = encoding::vcard::parse_all("BEGIN:VCARD\nVERSION:4.0\nFN:Ann\nEND:VCARD\nBEGIN:VCARD\nVERSION:4.0\nFN:Bob\nEND:VCARD\n");
    for (const auto& card : book.value()) {
        println(card.formatted_name());
    }
}
```

Output:

```text
Ann
Bob
```

## See also

- [parse](parse.md)
- [sgcl::encoding::vcard](README.md)
