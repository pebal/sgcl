[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml/README.md)

# sgcl::encoding::xml::reader

```cpp
#include "sgcl/encoding/xml.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class xml {
    public:
        class reader;
    };
}
```

`sgcl::encoding::xml::reader` reads XML a token at a time — Go's `xml.Decoder` — over a document in memory or a
stream that brings it a piece at a time. [next](next.md) gives the next [token](../xml-token/README.md),
[peek](peek.md) the one `next()` will give, [read](read.md) the next node whole (an element
with everything inside it, as a [tree](../xml/README.md), or as a value of a program's type), [skip](skip.md)
passes over it. Nothing of the document is held but the token being read, so a feed of a million entries is read
in the memory of one: peek at each start, read the entries wanted, step over the rest.

Everything the [tree](../xml/README.md#rules) says of what is well formed holds here: no DTD, the five entities, the limits
of [options](../xml-options.md) (`max_depth`, `max_token_size`), namespaces, the encodings, the errors with their
place.

## Rules

- **Errors are a state, not a result per call**: `next()` gives `nullopt` at the end of the document and on an
  error, which [last_error](last_error.md) then keeps — a loop reads as a loop, as with
  `buffered_reader::lines()`. After an error every call gives `nullopt` (`false` from `skip()`). The error has the
  byte of the input, the line, the column in characters and the path of the elements open (`"/feed/entry"`).
- **`read()` and `skip()` take the next node**: an element whole, a text (the pieces of one text joined), and the
  comments and instructions a tree keeps. They leave out what a tree leaves out — comments without
  `options::keep_comments`, white space alone without `options::keep_whitespace` — and the XML and DOCTYPE
  declarations. At the end tag of the element they are inside they give `nullopt` and `false` and leave the end
  tag for `next()`, so that `while (auto child = r.read())` goes over the children of the element whose start was
  the last token.
- **A stream** is read into a buffer of the reader's own, which grows to hold a token and no more (up to
  `options::max_token_size`). A token that arrives a byte at a time is followed by a finder that looks at each new
  byte once for where the token ends, and is read once that end is there: a reader of a slow stream does not read
  a large token again with every byte that comes, which would be quadratic. The stream failing ends the reading
  after the tokens it brought: `last_error()` has `errc::io` and the stream's own error in `io_error()`.
- **On a thread and in a task**: `next()`, `peek()`, `read()` and `skip()` read the stream on the thread that
  calls them; `async_next()`, `async_peek()`, `async_read()` and `async_skip()` give the worker back while the
  stream waits. A document in memory never waits.
- **`read<T>()`** gives the next element as a value of a program's type, mapped as
  [A program's types](../xml/README.md#a-programs-types) says. An element that is not a `T` — or text where an element was
  expected — stops the reader, and `last_error()` has the path inside the element and the offset of its start. A
  document of any length is read a value at a time in the memory of one element.
- [offset](offset.md) is the byte of the input where the next token starts;
  [depth](depth.md) the elements open around it.
- A reader holds tracked pointers (its buffer, its strings, the stream): it lives where a `tracked_ptr` may. It
  is moved, not copied.

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| `xml.NewDecoder(r)`, `Token()` | `xml::reader(in)`, `next()`: `nullopt` and `last_error()` in place of `(nil, err)`; `async_next()` in a task. Always strict, no DTD, UTF-16 and the single byte encodings built in |
| `Decoder.DecodeElement(&v, &start)` | `peek()` then `read<T>()`, or `read()` for a tree |
| `Decoder.Skip()` | `skip()`: the next node, not the rest of the current element |
| `Decoder.InputOffset`, `InputPos` | `offset()`; the error's `line()`, `column()` |
| `Decoder.RawToken` | none: names are always resolved; the prefix stays in the token's `name()` |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](xml-reader.md) | a reader of a text or of a stream, or one taken over |
| `(destructor)` | drops the reader; its buffer and strings are left to the collector |
| [operator=](operator_assign.md) | takes another reader over |

#### Reading

| Function | Description |
|---|---|
| [next, async_next](next.md) | the next token |
| [peek, async_peek](peek.md) | the token `next()` gives next, left where it is |
| [read, async_read](read.md) | the next node whole, or the next element as a value of a program's type |
| [skip, async_skip](skip.md) | passes over the node `read()` would give |

#### Observers

| Function | Description |
|---|---|
| [last_error](last_error.md) | what stopped the reader |
| [offset](offset.md) | the byte of the input where the next token starts |
| [depth](depth.md) | the elements open around the next token |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

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
            println("{}<{}> in {}", string(tokens.depth(), ' '), t->local_name(),
                    t->namespace_uri());
        }
    }

    // the entries whole, the rest stepped over
    encoding::xml::reader r(feed);
    while (auto t = r.peek()) {
        if (t->is_start("{http://www.w3.org/2005/Atom}entry")) {
            encoding::xml entry = r.read().value();
            println("{}: {}", entry.child("id").text(), entry.child("title").text());
        } else {
            r.next();
        }
    }

    encoding::xml::reader bad("<feed>\n  <entry>&nbsp;</entry>\n</feed>");
    while (bad.next()) {
    }
    println(bad.last_error()->message());
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

## See also

- [token](../xml-token/README.md): what `next()` gives
- [xml](../xml/README.md): the tree, and `parse` of a whole document
- [writer](../xml-writer/README.md): XML onto a stream
- [error](../error/README.md); [buffered_reader](../../io/buffered_reader/README.md)
- [sgcl::encoding::xml](../xml/README.md)
