# sgcl::encoding::xml

```cpp
#include "sgcl/encoding/xml.h"   // or "sgcl/encoding/encoding.h"

namespace sgcl::encoding {
    class xml;            // a node of a tree: an element, a text, a comment or an instruction
    class xml::builder;   // an element made a child at a time
}
```

XML 1.0 (fifth edition) with Namespaces in XML 1.0. `xml` is one node of a tree — an element with its attributes and children, a text, a comment or a processing instruction — and a **value that never changes**, as [`json`](README.md) is and the containers of [`immutable`](../immutable/README.md) are: a copy is a copy of the handle, a node is read by many threads without a lock, and a change makes a new node (`set`, `erase`, `push_back`) while the old one stays as it was. `xml::parse` reads a document into a tree and `to_string` writes one back; for a document read a token at a time, or one too large to hold, there is [`xml::reader`](xml-reader.md), and for writing onto a stream, [`xml::writer`](xml-writer.md).

A program's own types are read and written through the same `describe(field_list&)` JSON uses ([`field_list`](fields.md)): `xml::parse<T>`, `as<T>`, `from`, `stringify`, `reader::read<T>`, `writer::value`. Go's `encoding/xml` has no tree: it maps a document onto a program's structures (`xml.Unmarshal`) or hands out its tokens.

## Rules

- **No DTD is read.** A DOCTYPE declaration is checked for its shape and passed over — its internal subset skipped, never interpreted — so no entity exists but the five of XML (`&lt;` `&gt;` `&amp;` `&apos;` `&quot;`) and the character references. A reference to any other (`&nbsp;`) is `errc::undefined_entity`. Nothing is ever loaded from outside the document, and an entity that expands into more entities ("billion laughs") cannot exist: the attacks on a reader of XML (XXE, billion laughs, the quadratic blowup) have nothing to work on, by construction rather than by a setting. No default values of attributes either, and every attribute is of the type CDATA, which only a DTD could change.
- **The limits for a document from outside**: elements nested deeper than `options::max_depth` (512) are `errc::depth_limit`; a tag, a text or a comment of a stream held longer than `options::max_token_size` (16 MB) is `errc::out_of_range`. Nothing else grows with the input but the tree itself; for a document of any size, read it with [`xml::reader`](xml-reader.md) a node at a time.
- **Well formed or refused**: everything XML 1.0 and Namespaces in XML refuse is an error, with the byte, the line, the column (in characters) and the path of the elements open (`3:5 /catalog/book: undefined entity &nbsp; ...`). A prefix used and not declared, a name that is not a qualified name, two attributes of one name or of one namespace and local name, a second root, text outside the root, `]]>` in text, a character XML does not hold, invalid UTF-8.
- **The encodings**: UTF-8, UTF-16 (either order, found by its byte order mark or by the `<?` of its declaration) and the 27 single byte encodings of [`txt::encoding`](../txt/encoding.md) (ISO-8859-*, windows-125*, KOI8, Mac…) named by the declaration (`encoding="ISO-8859-2"`); what the document holds is UTF-8 after that. Another encoding (Shift_JIS, UTF-32) is `errc::unsupported_encoding`, as is a declaration that contradicts the byte order mark. The offset of an error counts the bytes of the input as it was.
- **Names**: `name()` is the name as the document writes it (`svg:rect`), `local_name()` the part after the prefix, `namespace_uri()` what the prefix — or the default namespace — stood for where the element was read. Where a method takes a name (`child`, `children`, `attribute`), it is matched as written, or by namespace and local name written `{http://www.w3.org/2000/svg}rect` (an attribute of no namespace is `{}id`).
- **What a tree holds**: elements, texts, and instructions; comments with `options::keep_comments`; white space alone between elements with `options::keep_whitespace` — without it, text made only of white space is left out, so that a document indented for the eye gives the same tree as one written on a line (text with anything else in it is kept whole, its white space too). The pieces of one text — around a reference, a CDATA section, a comment left out — are one text node. The XML declaration and the DOCTYPE are not nodes; nor is anything outside the root: `parse` gives the root element, having read and checked the rest.
- **Line endings** are `"\n"` everywhere (XML 1.0 2.11). **Attribute values** are normalized (3.3.3): a literal tab, line feed or carriage return is a space; a character reference to one is that character.
- **`xml()`** is no node (`kind::none`): what `child()` gives when there is no such child, so that `doc.child("a").child("b").text()` needs no check at each step; its text is empty, it has no attributes and no children.
- **Changing**: `set`, `erase` and `push_back` copy the node they change (its attribute and children arrays; the children themselves are shared) — a push_back in a loop is quadratic. For an element of many children use `xml::builder`.
- **Writing**: `to_string()` escapes what must be escaped (`&lt;` `&amp;` `&gt;`, in values `&quot;` and the white space a reader would normalize away, `&#xD;` for a carriage return), writes an element without content as `<empty/>`, and a character XML cannot hold (a control, invalid UTF-8) as U+FFFD, as Go writes it. `xml::pretty` indents elements two spaces a level, except where an element holds text: its content is written as it is, since there the white space is content. What `to_string` writes, `parse` reads back as the same tree.
- **What is a program's mistake, not the input's, throws**: a name that is not a qualified name, a comment holding `--` or ending with `-`, an instruction's target `xml` or data holding `?>`, `set`, `erase` or `push_back` on a node that is not an element, `push_back(xml())` — `invalid_argument`.
- **A program's types** (`describe(field_list&)`), mapped as Go maps a structure: a field is a child element of its name — the name as written (`dc:title`) or `{namespace}local` to match whatever the prefix; a field marked `attribute()` is an attribute, one marked `text()` the element's own text. A number, a boolean (`true`/`false`, read also as `1`/`0`), a string, an enum (by its `names()` or its value) or a type with `to_text`/`from_text` is text; numbers and booleans are read with the white space around them left out, strings as they are; a float's infinities and NaN are `INF`, `-INF`, `NaN`. A sequence or a set (`vector`, `list`, `set`, `array<T, N>`, the immutable ones) is the element repeated, once for each value; an optional or a `tracked_ptr` is an element (or an attribute) that may be absent; a type with `describe()` is an element holding its fields. Elements come in any order, those of a sequence among others; an element or attribute the type has no field for is passed over. `required()` makes a missing one `errc::missing_field`, `omit_empty()` leaves an empty one out when written.
- **What has no form in XML here**: a map, a tuple, a variant, a list of lists, a `json` value, a record in an attribute — `errc::unsupported_value` when written, `errc::type_mismatch` when read; a value nested deeper than 512 elements (a cycle of `tracked_ptr`) is `errc::unsupported_value`.
- **The mapping goes through a tree**: the element is read whole into nodes and the nodes mapped. Its error has the path inside (`/catalog/book[2]/price`, indexes from 1 as XPath counts, `@id` for an attribute, `text()` for the text) and the place where the element read began: the root for `parse<T>`, the element for `reader::read<T>`.
- A tree is walked with loops of its own, not recursion: `to_string`, `text()`, `==` and parsing take a tree deeper than any thread's stack.
- A node holds tracked pointers: it lives where a `tracked_ptr` may (a stack, a managed object, a managed container).

## Members

```cpp
class xml {
public:
    using error = encoding::error;
    enum class kind : uint8_t { none, element, text, comment, instruction };

    struct attribute {
        string name;            // as written: "xlink:href"
        string value;           // references replaced, white space normalized
        string namespace_uri;   // "" for no prefix; xmlns attributes: http://www.w3.org/2000/xmlns/
    };

    struct options {
        uint32_t max_depth = 512;
        size_t max_token_size = 16 << 20;   // of a stream
        bool keep_comments = false;
        bool keep_whitespace = false;
    };

    struct style {
        uint8_t indent = 0;
        bool declaration = false;           // <?xml version="1.0" encoding="UTF-8"?> in front
    };
    static const style compact;             // {0, false}
    static const style pretty;              // {2, false}

    class token;                            // xml-reader.md
    class reader;                           // xml-reader.md
    class writer;                           // xml-writer.md
    class builder;

    // making
    xml() noexcept;                                         // no node
    explicit xml(const string& name);                       // <name/>
    xml(const string& name, const string& text);            // <name>text</name>
    static xml text_node(const string& text);
    static xml comment(const string& text);
    static xml instruction(const string& target, const string& data = {});

    // reading: the root element
    static expected<xml, error> parse(const string& text);
    static expected<xml, error> parse(const string& text, const options& o);
    static expected<xml, error> parse(const io::reader& in);
    static expected<xml, error> parse(const io::reader& in, const options& o);
    static async::task<expected<xml, error>> async_parse(const io::reader& in);
    static async::task<expected<xml, error>> async_parse(io::reader in, options o);

    // what it is
    kind type() const noexcept;
    bool exists() const noexcept;
    bool is_element() const noexcept;
    bool is_text() const noexcept;
    string name() const noexcept;             // "svg:rect"; an instruction's target
    string local_name() const noexcept;       // "rect"
    string namespace_uri() const noexcept;    // "http://www.w3.org/2000/svg"

    // inside
    optional<string> attribute(const string& name) const;   // "id", "xlink:href", "{uri}local"
    slice<const attribute> attributes() const noexcept;
    slice<const xml> children() const noexcept;
    xml child(const string& name) const;                    // the first, or xml()
    generator<xml> children(const string& name) const;      // every one of that name
    string text() const;                                    // of an element: its subtree's, joined

    // a program's types: describe(field_list&)
    template<class T> static expected<T, error> parse(const string& text);
    template<class T> static expected<T, error> parse(const string& text, const options& o);
    template<class T> static expected<T, error> parse(const io::reader& in);
    template<class T> static expected<T, error> parse(const io::reader& in, const options& o);
    template<class T> static async::task<expected<T, error>> async_parse(const io::reader& in);
    template<class T> static async::task<expected<T, error>> async_parse(io::reader in, options o);
    template<class T> expected<T, error> as() const;                          // this element as a T
    template<class T> static expected<xml, error> from(const string& name, const T& value);
    template<class T> static expected<string, error> stringify(const string& name, const T& value, const style& s = compact);

    // new versions
    xml set(const string& name, const string& value) const;
    xml erase(const string& name) const;
    xml push_back(const xml& child) const;

    string to_string(const style& s = compact) const;
    friend bool operator==(const xml& a, const xml& b);     // attributes in any order
};

class xml::builder {
public:
    explicit builder(const string& name);
    builder& set(const string& name, const string& value);
    builder& push_back(const xml& child);
    xml build();                                            // the builder is left empty
};
```

`parse(in)` reads the stream on the thread that calls it; `async_parse(in)` in a task gives the worker back while the stream waits.

## Example

```cpp
#include "sgcl/core/range.h"
#include "sgcl/encoding/xml.h"
#include "sgcl/io/os.h"

using namespace sgcl;

int main() {
    string text = R"(<?xml version="1.0"?>
<catalog xmlns:dc="http://purl.org/dc/elements/1.1/">
  <book id="1"><dc:title>Lalka</dc:title><price>39.90</price></book>
  <book id="2"><dc:title>Solaris &amp; more</dc:title></book>
</catalog>)";
    auto doc = encoding::xml::parse(text);
    if (!doc) {
        io::stdout.write(doc.error().message() + "\n");
        return 1;
    }
    for (auto book : doc->children("book")) {
        string id = book.attribute("id").value_or("?");
        string title = book.child("{http://purl.org/dc/elements/1.1/}title").text();
        string price = book.child("price").text();   // "" when there is none
        io::stdout.write(id + ": " + title + " [" + price + "]\n");
    }

    // a change is a new tree; the one read stays as it was
    encoding::xml added = doc->push_back(encoding::xml("book").set("id", "3").push_back(encoding::xml("dc:title", "Diuna")));
    io::stdout.write(added.to_string(encoding::xml::pretty) + "\n");

    encoding::xml::builder list("list");
    for (auto i : range(3)) {
        list.push_back(encoding::xml("item", string(std::to_string(i * i))));
    }
    io::stdout.write(list.build().to_string() + "\n");

    auto bad = encoding::xml::parse("<a>\n  <b>&nbsp;</b>\n</a>");
    io::stdout.write(bad.error().message() + "\n");

    // a program's own type, described once for every format
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
    auto dune = encoding::xml::parse<book>("<book id='7'><tag>sf</tag><title>Dune</title><tag>classic</tag></book>").value();
    dune.price = 45.5;
    io::stdout.write(encoding::xml::stringify("book", dune).value() + "\n");
    auto wrong = encoding::xml::parse<book>("<book id='8'>\n  <title>X</title>\n  <price>cheap</price>\n</book>");
    io::stdout.write(wrong.error().message() + "\n");
}
```

Output:

```text
1: Lalka [39.90]
2: Solaris & more []
<catalog xmlns:dc="http://purl.org/dc/elements/1.1/">
  <book id="1">
    <dc:title>Lalka</dc:title>
    <price>39.90</price>
  </book>
  <book id="2">
    <dc:title>Solaris &amp; more</dc:title>
  </book>
  <book id="3">
    <dc:title>Diuna</dc:title>
  </book>
</catalog>
<list><item>0</item><item>1</item><item>4</item></list>
2:6 /a/b: undefined entity &nbsp; (only the five of XML are known: no DTD is read)
<book id="7"><title>Dune</title><tag>sf</tag><tag>classic</tag><price>45.5</price></book>
1:1 /book/price: expected a number, found "cheap"
```

## SGCL and Go

| Go | SGCL | note |
|---|---|---|
| `xml.Unmarshal` into `any`-like structures | `encoding::xml::parse(text)` → a tree | Go has no tree |
| `xml.Unmarshal(b, &v)`, `xml.Marshal(v)` of a struct | `encoding::xml::parse<T>(text)`, `stringify("name", v)` | `describe(field_list&)` for the tags: `attr` → `attribute()`, `chardata` → `text()`; the root's name is given, not an `XMLName` field |
| `,innerxml`, `,any`, `,comment` | — | a field of type `xml` is not a kind of `fields.h` yet |
| `xml.Marshal`, `MarshalIndent` of a structure | `to_string()`, `to_string(xml::pretty)` of a tree | |
| `xml.Name{Space, Local}` | `namespace_uri()`, `local_name()`; `"{space}local"` where a name is asked for | |
| `Decoder.Strict = true` (the default) | always strict | Go reads a second root, text around the root, an unbound prefix, two attributes of one name, invalid UTF-8 |
| `Decoder.Entity` (a map of entities) | — | no entities but the five: no DTD, by design |
| `Decoder.CharsetReader` | built in | UTF-16 and the 27 single byte encodings of `txt` |
| `Decoder.AutoClose`, `Strict = false` (HTML-like input) | — | XML only; HTML is another grammar |

## The oracles

The **W3C XML Conformance Test Suite** (xmlconf 2013-09-23, in `~/Programming/oracles/xmlconf`, not in the repository): of its 1965 tests of XML 1.0 fifth edition and Namespaces 1.0, 1536 are read as the suite says; the other 429 are decided otherwise by design, each named with its reason in `tests/encoding/xml_conformance_expected.h` — 317 documents whose error lies in the internal subset of their DTD (skipped, not read), 29 whose error lies in an external DTD or entity (never loaded), 83 valid documents referring to an entity their DTD declares. Of the 309 valid documents with a canonical form, 251 are written in it as the suite writes them; the other 58 want an attribute default or type, or a notation, from their DTD. **Go's `encoding/xml`** gives the same tokens for the 1192 documents of the suite both read and for 32 documents named in `tools/xml_oracle.go`, which lists where the two are compared otherwise (Go keeps line endings in comments, takes a UTF-8 byte order mark for text, keeps white space in attribute values). 200,000 documents made by mutating those are read whole and through a stream in pieces of 3 bytes, and every one read is written and read again as the same tree; where Go also reads one, its tokens are Go's.

## See also

[`xml::reader`, `xml::token`](xml-reader.md); [`xml::writer`](xml-writer.md); [`field_list`](fields.md); [`error`](error.md); [`txt::encoding`](../txt/encoding.md).
