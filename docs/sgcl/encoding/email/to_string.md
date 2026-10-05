[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::to_string

```cpp
string to_string() const;                          // (1)
string to_string(const write_options& o) const;    // (2)
```

The message as text: the head's fields in their order (MIME-Version: 1.0 before the content fields when it has
none), folded at 78 characters, the body with each part's transfer encoding, CRLF line breaks.

1. For a transport of 7 bits: every byte ASCII, the Bcc field written.
2. As `o` says ([write_options](../email-write_options.md)): 8bit content, UTF-8 in the head, the Bcc or not.

A boundary is drawn at random for each multipart that has none and checked against what is written inside it; a
parsed multipart keeps its own when the content does not hold it.

## Parameters

| Parameter | Description |
|---|---|
| `o` | how the message is written |

## Return value

The text.

## Complexity

Linear in the size of the message.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;


int main() {
    encoding::email m("a@example.com", "b@example.com", "Hi", "Treść");
    m.set_date(time::datetime::from_unix(1791203400, time::zone::utc()));
    m.set_message_id("1@example.com");

    m.add_bcc("hidden@example.com");
    encoding::email::write_options o;
    o.allow_8bit = true;
    o.write_bcc = false;
    println("{}", m.to_string(o));
}
```

Output:

```text
Date: Mon, 05 Oct 2026 12:30:00 +0000
Message-ID: <1@example.com>
From: a@example.com
To: b@example.com
Subject: Hi
MIME-Version: 1.0
Content-Type: text/plain; charset=utf-8
Content-Transfer-Encoding: 8bit

Treść
```

## See also

- [write_to](write_to.md), [save](save.md)
- [write_options](../email-write_options.md)
- [email](README.md)
