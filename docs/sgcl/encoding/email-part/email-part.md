[sgcl](../../README.md) › [encoding](../README.md) › [email](../email/README.md) › [part](README.md)

# sgcl::encoding::email::part::part

```cpp
part(const string& content_type, const string& text);               // (1)
template<class T>
part(const string& content_type, const T& text);                    // (2)
part(const string& content_type, const slice<const byte>& data);    // (3)
explicit part(const string& content_type);                          // (4)
part(const part& other) noexcept;                                   // (5)
```

1. A part of a text, of the media type given (`"text/plain"`, `"text/calendar; method=REQUEST"`); a text type
   without a charset gets `charset=utf-8`, the text's line breaks are written CRLF.
2. A literal, a character array, a `std::string_view`: as (1).
3. A part of bytes, as they are.
4. An empty part of the type; a multipart (`"multipart/mixed"`) whose parts [add](add.md) gives.
5. The handle of `other`'s part.

## Parameters

| Parameter | Description |
|---|---|
| `content_type` | the media type with its parameters |
| `text` | the text |
| `data` | the bytes |
| `other` | the part whose handle is copied |

## Complexity

(1–4) Linear in the size of the content; (5) constant.

## Exceptions

- (1–4) `invalid_argument` when `content_type` is not a media type (`type/subtype`).
- (5) None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    encoding::email::part t("text/plain", "hello\n");
    println("{} {} {}", t.content_type(), t.charset(), t.content().size());
    encoding::email::part mixed("multipart/mixed");
    mixed.add(t);
    println("{} {}", mixed.is_multipart(), mixed.parts().size());
    try {
        encoding::email::part bad("nonsense", "x");
    } catch (const invalid_argument&) {
        println("invalid_argument");
    }
}
```

Output:

```text
text/plain utf-8 7
true 1
invalid_argument
```

## See also

- [add](add.md)
- [email::set_body](../email/set_body.md)
- [part](README.md)
