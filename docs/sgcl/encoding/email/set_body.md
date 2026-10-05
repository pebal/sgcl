[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::set_body

```cpp
email& set_body(const part& p);
```

A body of a program's own shape: the content fields (Content-*) and the content or parts of `p` become the
message's, its other fields kept. For what the calls of the body do not make: multipart/signed, a calendar's
invitation (text/calendar beside the text), a report.

## Parameters

| Parameter | Description |
|---|---|
| `p` | the body |

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
    encoding::email m("a@example.com", "b@example.com", "Meeting", "unused");
    encoding::email::part alt("multipart/alternative");
    alt.add(encoding::email::part("text/plain", "You are invited."));
    encoding::email::part invite("text/calendar; method=REQUEST", "BEGIN:VCALENDAR\nEND:VCALENDAR");
    alt.add(invite);
    m.set_body(alt);
    println("{} | {}", m.body().content_type(), m.text());
}
```

Output:

```text
multipart/alternative | You are invited.
```

## See also

- [body](body.md)
- [part](../email-part/README.md)
- [email](README.md)
