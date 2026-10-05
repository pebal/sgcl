[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::set_subject

```cpp
email& set_subject(const string& text);
```

The Subject field: any text, written with the words that need it as encoded words (RFC 2047) and folded at 78
characters.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the subject |

## Return value

`*this`.

## Complexity

Linear in the number of fields.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    encoding::email m;
    m.set_subject("Raport — zażółć");
    auto text = m.to_string();
    size_t at = text.view().find("Subject:");
    println("{}", text.view().substr(at, text.view().find('\r', at) - at));
    println("{}", encoding::email::parse(text)->subject());
}
```

Output:

```text
Subject: Raport =?utf-8?b?4oCUIHphxbzDs8WCxIc=?=
Raport — zażółć
```

## See also

- [subject](subject.md)
- [email](README.md)
