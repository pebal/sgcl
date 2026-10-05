[sgcl](../../README.md) › [encoding](../README.md) › [email](README.md)

# sgcl::encoding::email::email

```cpp
email();                                                              // (1)
email(const string& from, const string& to, const string& subject,    // (2)
      const string& text);
email(const email& other) noexcept;                                   // (3)
```

1. An empty message: a Date of now, a Message-ID, a body of empty text.
2. A message of a text: From of `from` (one address, `"Alice <alice@example.com>"`), To of `to` (a list,
   `"a@x, B <b@y>"`), the subject and the text (line breaks of any kind, written as CRLF), with (1)'s Date and
   Message-ID, the Message-ID at the domain of `from`.
3. The handle of `other`'s message: the same message.

## Parameters

| Parameter | Description |
|---|---|
| `from` | the sender, an address as RFC 5322 writes one |
| `to` | the recipients, an address list |
| `subject` | the subject, any text |
| `text` | the text of the body |
| `other` | the message whose handle is copied |

## Complexity

(1–2) Linear in the size of the texts; (3) constant.

## Exceptions

- (2) `invalid_argument` when `from` is not one address or `to` not an address list.
- (1, 3) None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;


int main() {
    string to = "bob@example.org, Carol <carol@example.net>";
    encoding::email m("Alice <alice@example.com>", to, "Hi", "Hello.");
    println("{} | {}", m.from()->to_string(), m.to().size());
    println("{}", m.message_id().view().ends_with("@example.com>"));
    encoding::email same = m;
    println("{}", same == m);
    try {
        encoding::email bad("no address", "bob@example.org", "Hi", "Hello.");
    } catch (const invalid_argument&) {
        println("invalid_argument");
    }
}
```

Output:

```text
Alice <alice@example.com> | 2
true
true
invalid_argument
```

## See also

- [set_from](set_from.md), [add_to](add_to.md), [set_text](set_text.md)
- [parse](parse.md): a message read
- [email](README.md)
