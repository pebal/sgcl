# sgcl::encoding::xml::reader

```cpp
#include "sgcl/encoding/xml.h"   // or "sgcl/encoding/encoding.h"

namespace sgcl::encoding {
    class xml::reader;   // the tokens of a document, from a string or a stream
    class xml::token;    // one of them
}
```

A reader of XML a token at a time — Go's `xml.Decoder` — over a document in memory or a stream that brings it a piece at a time. `next()` gives the next token, `peek()` the one `next()` will give, `read()` the next node whole (an element with everything inside it, as a [tree](xml.md)), `skip()` passes over it. Nothing of the document is held but the token being read, so a feed of a million entries is read in the memory of one: peek at each start, `read()` the entries wanted, step over the rest.

Everything the [tree](xml.md) says of what is well formed holds here: no DTD, the five entities, the limits of `options` (`max_depth`, `max_token_size`), namespaces, the encodings, the errors with their place.

## Rules

- **Tokens**: the start of an element (its name, namespace and attributes), its end, a run of text, a comment, a processing instruction, the DOCTYPE declaration. `<a/>` is a start and an end. The XML declaration is an instruction named `xml` (`text()` is `version="1.0" encoding="UTF-8"`), as Go gives it. A CDATA section is text, and the text of an element may come in more than one token: before and after a CDATA section or a comment (Go splits it the same way). White space between elements is text; outside the root, only white space is allowed.
- **A token is a value**: its names, attributes and text are strings of its own, kept as long as it is — after the reader has moved on, in a container, in another thread. The short strings (names, the white space between elements, short values) are shared across the document: a thousand `<book>` elements hold one `"book"`.
- **Errors are a state, not a result per call**: `next()` gives `nullopt` at the end of the document and on an error, which `last_error()` then keeps — a loop reads as a loop, as with `buffered_reader::lines()`. After an error every call gives `nullopt` (`false` from `skip()`). The error has the byte of the input, the line, the column in characters and the path of the elements open (`"/feed/entry"`).
- **`read()` and `skip()`** take the next node: an element whole, a text (the pieces of one text joined), and the comments and instructions a tree keeps. They leave out what a tree leaves out — comments without `options::keep_comments`, white space alone without `options::keep_whitespace` — and the XML and DOCTYPE declarations. At the end tag of the element they are inside they give `nullopt` and `false` and leave the end tag for `next()`, so that `while (auto child = r.read())` goes over the children of the element whose start was the last token.
- **A stream** is read into a buffer of its own, which grows to hold a token and no more (up to `options::max_token_size`). A token that arrives a byte at a time is followed by a finder that looks at each new byte once for where the token ends, and is read once that end is there: a reader of a slow stream does not read a large token again with every byte that comes, which is quadratic. The stream failing ends the reading after the tokens it brought: `last_error()` has `errc::io` and the stream's own error in `io_error()`.
- **On a thread and in a task**: `next()`, `peek()`, `read()` and `skip()` read the stream on the thread that calls them; `async_next()`, `async_peek()`, `async_read()` and `async_skip()` give the worker back while the stream waits. A document in memory never waits.
- **`read<T>()`**: the next element as a value of a program's type ([`xml`](xml.md) says how a type is mapped), `nullopt` where `read()` gives it; an element that is not a `T` — or text where an element was expected — stops the reader, and `last_error()` has the path inside the element and the offset of its start. A document of any length is read a value at a time in the memory of one element.
- `offset()` is the byte of the input where the next token starts; `depth()` the elements open around it.
- A reader holds tracked pointers (its buffer, its strings, the stream): it lives where a `tracked_ptr` may. It is moved, not copied.

## Members

```cpp
class xml::reader {
public:
    explicit reader(const string& text);
    reader(const string& text, const options& o);
    explicit reader(const io::reader& in);
    reader(const io::reader& in, const options& o);

    optional<token> next();
    optional<token> peek();
    optional<xml> read();
    bool skip();

    async::task<optional<token>> async_next();
    async::task<optional<token>> async_peek();
    async::task<optional<xml>> async_read();
    async::task<bool> async_skip();

    template<class T> optional<T> read();                    // the next element as a T
    template<class T> async::task<optional<T>> async_read();

    const optional<error>& last_error() const noexcept;
    uint64_t offset() const noexcept;
    uint32_t depth() const noexcept;
};

class xml::token {
public:
    enum class kind : uint8_t { start_element, end_element, text, comment, instruction, doctype };

    kind type() const noexcept;
    const string& name() const noexcept;            // "svg:rect"; an instruction's target; the DOCTYPE's root
    const string& local_name() const noexcept;
    const string& namespace_uri() const noexcept;
    slice<const xml::attribute> attributes() const noexcept;
    optional<string> attribute(const string& name) const;   // "id", "xlink:href", "{uri}local"
    const string& text() const noexcept;            // text, comment, instruction's data, the DOCTYPE after its keyword
    bool is_start(const string& name) const noexcept;
    bool is_end(const string& name) const noexcept;
};
```

## Example

```cpp
#include "sgcl/encoding/xml.h"
#include "sgcl/io/os.h"

using namespace sgcl;

int main() {
    string feed = R"(<?xml version="1.0" encoding="UTF-8"?>
<feed xmlns="http://www.w3.org/2005/Atom">
  <title>News</title>
  <entry><id>1</id><title>First</title></entry>
  <!-- an entry was here -->
  <entry><id>2</id><title>Second</title></entry>
</feed>)";

    // every token
    encoding::xml::reader tokens(feed);
    while (auto t = tokens.next()) {
        if (t->type() == encoding::xml::token::kind::start_element) {
            io::stdout.write(string(tokens.depth(), ' ') + "<" + t->local_name() + "> in " + t->namespace_uri() + "\n");
        }
    }

    // the entries whole, the rest stepped over
    encoding::xml::reader r(feed);
    while (auto t = r.peek()) {
        if (t->is_start("{http://www.w3.org/2005/Atom}entry")) {
            encoding::xml entry = r.read().value();
            io::stdout.write(entry.child("id").text() + ": " + entry.child("title").text() + "\n");
        } else {
            r.next();
        }
    }

    encoding::xml::reader bad(string("<feed>\n  <entry>&nbsp;</entry>\n</feed>"));
    while (bad.next()) {
    }
    io::stdout.write(bad.last_error()->message() + "\n");
}
```

Output:

```text
 <feed> in http://www.w3.org/2005/Atom
  <title> in http://www.w3.org/2005/Atom
  <entry> in http://www.w3.org/2005/Atom
   <id> in http://www.w3.org/2005/Atom
   <title> in http://www.w3.org/2005/Atom
  <entry> in http://www.w3.org/2005/Atom
   <id> in http://www.w3.org/2005/Atom
   <title> in http://www.w3.org/2005/Atom
1: First
2: Second
2:10 /feed/entry: undefined entity &nbsp; (only the five of XML are known: no DTD is read)
```

## SGCL and Go

| Go | SGCL | note |
|---|---|---|
| `xml.NewDecoder(r)`, `Token()` | `encoding::xml::reader(in)`, `next()` | `nullopt` and `last_error()` in place of `(nil, err)`; `async_next()` in a task |
| `StartElement`, `EndElement`, `CharData`, `Comment`, `ProcInst`, `Directive` | `token::kind::start_element`, `end_element`, `text`, `comment`, `instruction`, `doctype` | a token keeps its strings; Go's `CharData` is valid until the next call |
| `Decoder.DecodeElement(&v, &start)` | `peek()` then `read<T>()` (or `read()`, a tree) | |
| `Decoder.Skip()` | `skip()` | the next node, not the rest of the current element |
| `Decoder.InputOffset`, `InputPos` | `offset()`; the error's `line()`, `column()` | |
| `Decoder.RawToken` | — | names are always resolved; the prefix stays in `name()` |

## See also

[`xml`](xml.md), the tree; [`xml::writer`](xml-writer.md); [`error`](error.md); [`buffered_reader`](../io/buffered.md).
