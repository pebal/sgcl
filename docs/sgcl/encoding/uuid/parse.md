[sgcl](../../README.md) › [encoding](../README.md) › [uuid](README.md)

# sgcl::encoding::uuid::parse

```cpp
static expected<uuid, error> parse(const string& text) noexcept;
```

A UUID of a text from outside, in the forms Go's google/uuid and Python's `uuid` read: `8-4-4-4-12`, the same
in braces `{}`, after `urn:uuid:`, and 32 hex digits without hyphens; the digits in either case.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the UUID written |

## Return value

The UUID, or the [error](../error/README.md): `syntax` for a text of another length (at its end) or a hyphen
or a brace out of place (at it), `invalid_character` for a character that is not a hex digit (at it).

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
    for (const char* text : {"F81D4FAE-7DEC-11D0-A765-00A0C91E6BF6", "{f81d4fae-7dec-11d0-a765-00a0c91e6bf6}",
                             "f81d4fae7dec11d0a76500a0c91e6bf6", "f81d4fae-7dec-11d0-a765-00a0c91e6bfg",
                             "f81d4fae-7dec"}) {
        auto id = encoding::uuid::parse(text);
        if (id) {
            println(*id);
        } else {
            println(id.error().message());
        }
    }
}
```

Output:

```text
f81d4fae-7dec-11d0-a765-00a0c91e6bf6
f81d4fae-7dec-11d0-a765-00a0c91e6bf6
f81d4fae-7dec-11d0-a765-00a0c91e6bf6
offset 35: invalid character 'g' in a UUID
offset 13: not a UUID: its length is 13
```

## See also

- [to_string](to_string.md): the other way
- [(constructor)](uuid.md): a literal
- [sgcl::encoding::uuid](README.md)
