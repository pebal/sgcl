[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md)

# sgcl::encoding::xml::parse, async_parse

```cpp
static expected<xml, error> parse(const string& text);                                        // (1)
static expected<xml, error> parse(const string& text, const options& o);                      // (2)
static expected<xml, error> parse(const io::reader& in);                                      // (3)
static expected<xml, error> parse(const io::reader& in, const options& o);                    // (4)
static async::task<expected<xml, error>> async_parse(const io::reader& in) noexcept;          // (5)
static async::task<expected<xml, error>> async_parse(io::reader in, options o) noexcept;      // (6)
template<class T> static expected<T, error> parse(const string& text);                        // (7)
template<class T> static expected<T, error> parse(const string& text, const options& o);      // (8)
template<class T> static expected<T, error> parse(const io::reader& in);                      // (9)
template<class T> static expected<T, error> parse(const io::reader& in, const options& o);    // (10)
template<class T>
static async::task<expected<T, error>> async_parse(const io::reader& in) noexcept;            // (11)
template<class T>
static async::task<expected<T, error>> async_parse(io::reader in, options o) noexcept;        // (12)
```

Reads a document whole.

- (1–6) The root element of the document. What stands before and after it — the XML declaration, the DOCTYPE,
  comments, instructions, white space — is read and checked, and left out.
- (7–12) The root element as a value of a program's type `T`, described by `describe(field_list&)` and mapped as
  [A program's types](../xml.md#a-programs-types) says. The document is read into a tree and the tree mapped; `T`
  needs a default constructor.

1. and 7. The document in `text`, with the default [options](../xml-options.md).
2. and 8. The same with the options `o`.
3. and 9. The document a stream brings, read to its end on the thread that calls it, with the default options.
4. and 10. The same with the options `o`.
5. and 11. (3) and (9) in a task: `co_await xml::async_parse(in)` gives the worker back while the stream waits.
6. and 12. The same with the options `o`. The stream and the options are taken by value: a task runs after the
   call that made it, when the caller's temporaries are gone.

A document is well formed or refused: the error has the byte of the input, the line, the column in characters and
the path of the elements open ([error](../error.md)). An error of the mapping (7–12) has the path inside the root
(`/catalog/book[2]/@id`) and the place where the root began: its line and column for a text in memory (7–8), its
offset alone for a stream (9–12). Go's `xml.Unmarshal` reads into a structure only; (1–6) give a tree, which Go has
not.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the document, in UTF-8, UTF-16 or an encoding its declaration names |
| `in` | the stream that brings the document |
| `o` | what is accepted and kept: the limits, the comments, the white space |

## Return value

The root element (1–6), or the value of `T` (7–12); otherwise the [error](../error.md): `errc::syntax` and the
other codes of a document not well formed, `errc::undefined_entity` for an entity no DTD can define,
`errc::unsupported_encoding`, `errc::depth_limit` and `errc::out_of_range` past the limits of `o`, `errc::io` when
the stream fails, its error in `io_error()`; and for (7–12) `errc::type_mismatch`, `errc::out_of_range` and
`errc::missing_field` of the mapping.

## Complexity

Linear in the length of the document; (7–12) and in the size of the tree mapped.

## Exceptions

- (1–4, 7–10) `length_error` when a string the reading makes would pass `string::max_size()`: a document in an
  encoding other than UTF-8 is turned into UTF-8 whole, a token is one string. (3–4, 9–10) What the read of the
  stream throws. (7–10) What the default constructor of `T` and its `describe` throw.
- (5–6, 11–12) None: what (3–4) and (9–10) throw, the task's `co_await` or `wait()` throws again.

## Example

A document that is not well formed:

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    auto bad = encoding::xml::parse("<a>\n  <b>&nbsp;</b>\n</a>");
    println(bad.error().message());
    println(bad.error().code() == encoding::errc::undefined_entity);

    encoding::xml::options deep;
    deep.max_depth = 2;
    println(encoding::xml::parse("<a><b><c/></b></a>", deep).error().message());
}
```

Output:

```text
2:6 /a/b: undefined entity &nbsp; (only the five of XML are known: no DTD is read)
true
1:7 /a/b: elements nested deeper than options.max_depth (2)
```

A program's own type, described once for every format:

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

struct book {
    string id;
    string title;
    vector<string> tags;
    optional<double> price;

    void describe(encoding::field_list& f) {
        f.add("id", id).attribute().required();
        f.add("title", title);
        f.add("tag", tags);
        f.add("price", price);
    }
};

int main() {
    auto dune = encoding::xml::parse<book>(
        "<book id='7'><tag>sf</tag><title>Dune</title><tag>classic</tag></book>").value();
    println("{} {} {} {}", dune.id, dune.title, dune.tags, dune.price.has_value());
    auto wrong = encoding::xml::parse<book>(
        "<book id='8'>\n  <title>X</title>\n  <price>cheap</price>\n</book>");
    println(wrong.error().message());
}
```

Output:

```text
7 Dune ["sf", "classic"] false
1:1 /book/price: expected a number, found "cheap"
```

A stream, on a thread and in a task:

```cpp
#include "sgcl/async.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

async::task<> count_items() {
    auto doc = co_await encoding::xml::async_parse(io::open("order.xml").value());
    int n = 0;
    for (auto item : doc->children("item")) {
        ++n;
    }
    println("{} items", n);
}

int main() {
    io::write_file("order.xml", "<order><item sku='a1'/><item sku='b2'/></order>").value();
    auto doc = encoding::xml::parse(io::open("order.xml").value());
    println(doc->child("item").attribute("sku", "?"));
    count_items().wait();
}
```

Output:

```text
a1
2 items
```

## See also

- [load](load.md): the root element of a file
- [reader](../xml-reader.md): a document a token at a time
- [to_string](to_string.md), [stringify](stringify.md): the way back
- [options](../xml-options.md), [error](../error.md)
- [sgcl::encoding::xml](../xml.md)
