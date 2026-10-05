[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::set_header

```cpp
email& set_header(const string& name, const string& value);
```

The field `name` with the text `value`, in the place of the first field of the name, the others of the name gone;
at the end when there was none. The value is encoded when the message is written: addresses as addresses,
other text with encoded words where it needs them, a line break in it as a space. Setting Message-ID keeps it
from moving with From.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the field's name: printable ASCII but the colon (RFC 5322 §3.6.8) |
| `value` | its text |

## Return value

`*this`.

## Complexity

Linear in the number of fields.

## Exceptions

`invalid_argument` when `name` is not a field name; the message is unchanged.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;


int main() {
    encoding::email m("a@example.com", "b@example.com", "s", "t");
    m.set_header("X-Note", "zażółć\r\nX-Injected: no");
    m.set_header("X-Note", "café");
    auto text = m.to_string();
    println("{}", encoding::email::parse(text)->header("X-Note"));
    println("{}", text.view().find("X-Injected") == std::string_view::npos);
}
```

Output:

```text
café
true
```

## See also

- [add_header](add_header.md)
- [remove_header](remove_header.md)
- [header](header.md)
- [email](README.md)
