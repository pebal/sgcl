[sgcl](../../README.md) › [encoding](../README.md)

# sgcl::encoding::email

```cpp
#include "sgcl/encoding/email.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class email {
    public:
        class address;
        class part;
        struct limits;
        struct write_options;
    };
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::email` is a mail message — the Internet Message Format of RFC 5322 with the MIME structure of
RFC 2045–2049 — built, written and parsed: Python's `email.message.EmailMessage`, what Go spreads over `net/mail`,
`mime`, `mime/multipart` and `mime/quotedprintable`. A message of a text is one line,
`encoding::email m(from, to, subject, text)`; [set_html](set_html.md), [attach](attach.md) and [embed](embed.md)
give it the shape every reader of mail knows (multipart/mixed of the body and the attachments, the body
multipart/alternative of the text and the HTML, the HTML multipart/related with its inline images), and
[to_string](to_string.md) writes it. [parse](parse.md) reads a message from its bytes; [text](text.md),
[html](html.md) and [attachments](attachments.md) take out what a reader looks for, [body](body.md) gives the
whole MIME tree as [parts](../email-part/README.md). [net::smtp](../../net/smtp/README.md) sends and receives it.

The head's fields are set and read by name ([header](header.md), [set_header](set_header.md)); the addresses
([from](from.md), [to](to.md), [address](../email-address/README.md)), the subject, the date and the Message-ID have
calls of their own. What is written follows the RFCs without being asked: text past ASCII in the head as encoded
words (RFC 2047), a file's name past ASCII by RFC 2231, a domain past ASCII by IDNA, the transfer encoding each
part's content needs (7bit, quoted-printable, base64; 8bit and UTF-8 in the head only where the transport takes
them, [write_options](../email-write_options.md)), lines folded at 78 characters, CRLF everywhere, a boundary drawn
at random and checked against the content. What is read is taken as the readers of mail take it: the obsolete
syntax of RFC 5322 §4, UTF-8 in the head (RFC 6532), LF for CRLF, encoded words where they should not be, a
multipart without its closing delimiter, the charsets [txt](../../txt/README.md) converts; only the
[limits](../email-limits.md) refuse a message.

## Rules

- A message is a handle, as an [http::request](../../net/http/request/README.md) is: one word, a tracked word to
  the message's tree; a copy is the same message, and what is changed through one is seen through the other
  ([operator==](operator_cmp.md) says whether two are one).
- The constructors set Date (now, in the local zone) and a Message-ID of random letters at the domain of From
  (`localhost` until there is a From; [set_from](set_from.md) moves a Message-ID the constructor made to the new
  domain, one set by the program is kept). A parsed message gets neither: it keeps what it came with.
- A field set by the program is kept as text and encoded when it is written: an address field
  (From, To, Cc, Bcc, Reply-To, Sender and their Resent- forms) as addresses, an id or a date as it is, any other
  as unstructured text with encoded words where it needs them; a line break in a value is a space, so no value
  ends its line. A field read from the wire is written back as it came.
- [set_text](set_text.md), [set_html](set_html.md), [attach](attach.md) and [embed](embed.md) take the body
  apart into its text, its HTML, its inline parts and its attachments, change one, and put it back in the shape
  above; a parsed body of another shape is put in that shape when one of them is called.
  [set_body](set_body.md) gives a body of a program's own shape.
- A part's content is kept with its transfer encoding undone; the writer chooses the encoding again, keeping a
  part's quoted-printable or base64 and its charset.
- Not thread-safe: one thread or task changes a message at a time; reading from several is fine.

## Member types

| Type | Definition |
|---|---|
| `error` | [encoding::error](../error/README.md) |
| [address](../email-address/README.md) | an address: a display name and an addr-spec |
| [part](../email-part/README.md) | a part of the MIME tree |
| [limits](../email-limits.md) | what a parse takes: the bytes of a head, the nesting, the parts |
| [write_options](../email-write_options.md) | how a message is written: 8bit, UTF-8 in the head, the Bcc |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](email.md) | an empty message, or one of a text from one address to others |
| `(destructor)` | drops the handle; the message is left to the collector |
| `operator=` | the handle of another message |

#### The head

| Function | Description |
|---|---|
| [header](header.md) | the first value of a field, decoded |
| [header_all](header_all.md) | every value of a field |
| [has_header](has_header.md) | whether there is a field of the name |
| [headers](headers.md) | every field in its order |
| [set_header](set_header.md) | a field's value, in place of the ones before |
| [add_header](add_header.md) | a field at the end |
| [remove_header](remove_header.md) | every field of a name taken out |

#### Addresses

| Function | Description |
|---|---|
| [from](from.md) | the address of From |
| [to](to.md) | the addresses of To |
| [cc](cc.md) | the addresses of Cc |
| [bcc](bcc.md) | the addresses of Bcc |
| [reply_to](reply_to.md) | the addresses of Reply-To |
| [set_from](set_from.md) | the address of From |
| [add_to](add_to.md) | an address or a list added to To |
| [add_cc](add_cc.md) | added to Cc |
| [add_bcc](add_bcc.md) | added to Bcc |
| [add_reply_to](add_reply_to.md) | added to Reply-To |

#### Subject, date, id

| Function | Description |
|---|---|
| [subject](subject.md) | the subject, decoded |
| [set_subject](set_subject.md) | the subject |
| [date](date.md) | the Date field as a datetime |
| [set_date](set_date.md) | the Date field |
| [message_id](message_id.md) | the Message-ID |
| [set_message_id](set_message_id.md) | the Message-ID |

#### The body

| Function | Description |
|---|---|
| [text](text.md) | the text of the body |
| [html](html.md) | the HTML of the body |
| [set_text](set_text.md) | the text of the body |
| [set_html](set_html.md) | the HTML of the body |
| [attach](attach.md) | a file or bytes as an attachment |
| [embed](embed.md) | a part the HTML shows by its Content-ID |
| [attachments](attachments.md) | the attachments |
| [body](body.md) | the root of the MIME tree |
| [set_body](set_body.md) | a body of a program's own shape |

#### Writing

| Function | Description |
|---|---|
| [to_string](to_string.md) | the message as text |
| [write_to](write_to.md) | the message to a writer |
| [save, async_save](save.md) | the message into a file |

#### Parsing

| Function | Description |
|---|---|
| [parse](parse.md) | a message read from text, bytes or a reader (static) |
| [load, async_load](load.md) | a message read from a file (static) |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | whether two handles are one message |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"
#include "sgcl/time.h"

using namespace sgcl;


int main() {
    encoding::email m("Łucja <lucja@example.pl>", "bob@example.org", "Zażółć", "Hi, Bob.\n");
    m.set_date(time::datetime::from_unix(1791203400, time::zone::utc()));
    m.set_message_id("1@example.com");

    print("{}", m.to_string());

    auto back = encoding::email::parse(m.to_string());
    println("{} | {} | {}", back->from()->name(), back->subject(), back->text().trim());
}
```

Output:

```text
Date: Mon, 05 Oct 2026 12:30:00 +0000
Message-ID: <1@example.com>
From: =?utf-8?b?xYF1Y2ph?= <lucja@example.pl>
To: bob@example.org
Subject: =?utf-8?b?WmHFvMOzxYLEhw==?=
MIME-Version: 1.0
Content-Type: text/plain; charset=utf-8

Hi, Bob.
Łucja | Zażółć | Hi, Bob.
```

## See also

- [net::smtp](../../net/smtp/README.md): sending and receiving messages
- [quoted_printable](../quoted_printable/README.md), [base64](../base64/README.md): the transfer encodings
- [time::email](../../time/layout/README.md): the date of a message
- RFC 5322, RFC 2045–2049, RFC 2047, RFC 2231, RFC 6532; `tests/encoding/email.cpp` (the RFCs' examples),
  `tests/encoding/email_oracle.cpp` (Python's `email` package and Go's `net/mail` and `mime` read what this writes,
  and this reads what they write)
