[sgcl](../../README.md) › [encoding](../README.md)

# sgcl::encoding::xml

```cpp
#include "sgcl/encoding/xml.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class xml;
}
```

`sgcl::encoding::xml` is one node of a tree of XML 1.0 (fifth edition) with Namespaces in XML 1.0 — an element
with its attributes and children, a text, a comment or a processing instruction — and a value that never changes,
as [json](../json/README.md) is and the containers of [immutable](../../immutable/README.md) are: a copy is a copy of the
handle, one word; a node is read by many threads without a lock; a change makes a new node
([set](set.md), [erase](erase.md), [push_back](push_back.md)) and the old one stays as it was.

[parse](parse.md) reads a document into a tree and [to_string](to_string.md) writes one back;
[load](load.md) and [save](save.md) do the same with a file. For a document read a token at a time, or one
too large to hold, there is [xml::reader](../xml-reader/README.md); for writing onto a stream, [xml::writer](../xml-writer/README.md);
for an element of many children, [xml::builder](../xml-builder/README.md).

A program's own types are read and written through the same `describe(field_list&)` JSON uses
([field_list](../field_list/README.md)): `parse<T>`, [as](as.md), [from](from.md), [stringify](stringify.md),
`reader::read<T>`, `writer::value`. Go's `encoding/xml` has no tree: it maps a document onto a program's
structures (`xml.Unmarshal`) or hands out its tokens.

## Rules

- **No DTD is read.** A DOCTYPE declaration is checked for its shape and passed over — its internal subset
  skipped, never interpreted — so no entity exists but the five of XML (`&lt;` `&gt;` `&amp;` `&apos;` `&quot;`)
  and the character references. A reference to any other (`&nbsp;`) is `errc::undefined_entity`. Nothing is ever
  loaded from outside the document, and an entity that expands into more entities ("billion laughs") cannot
  exist: the attacks on a reader of XML (XXE, billion laughs, the quadratic blowup) have nothing to work on, by
  construction rather than by a setting. No default values of attributes either, and every attribute is of the
  type CDATA, which only a DTD could change.
- **The limits for a document from outside** are those of [options](../xml-options.md): elements nested deeper than
  `max_depth` (512) are `errc::depth_limit`; a tag, a text or a comment of a stream held longer than
  `max_token_size` (16 MB) is `errc::out_of_range`. Nothing else grows with the input but the tree itself; a
  document of any size is read with [xml::reader](../xml-reader/README.md) a node at a time.
- **Well formed or refused**: everything XML 1.0 and Namespaces in XML refuse is an [error](../error/README.md), with the
  byte, the line, the column (in characters) and the path of the elements open
  (`3:5 /catalog/book: undefined entity &nbsp; ...`): a prefix used and not declared, a name that is not a
  qualified name, two attributes of one name or of one namespace and local name, a second root, text outside the
  root, `]]>` in text, a character XML does not hold, invalid UTF-8.
- **The encodings**: UTF-8, UTF-16 (either order, found by its byte order mark or by the `<?` of its
  declaration) and the 27 single byte encodings of [txt::encoding](../../txt/encoding.md) (ISO-8859-\*,
  windows-125\*, KOI8, Mac…) named by the declaration (`encoding="ISO-8859-2"`); what the document holds is UTF-8
  after that. Another encoding (Shift_JIS, UTF-32) is `errc::unsupported_encoding`, as is a declaration that
  contradicts the byte order mark. The offset of an error counts the bytes of the input as it was.
- **Names**: [name](name.md) is the name as the document writes it (`svg:rect`),
  [local_name](local_name.md) the part after the prefix, [namespace_uri](namespace_uri.md) what the
  prefix — or the default namespace — stood for where the element was read. Where a method takes a name
  ([child](child.md), [children](children.md), [attribute](attribute.md)), it is matched as written,
  or by namespace and local name written `{http://www.w3.org/2000/svg}rect` (an attribute of no namespace is
  `{}id`).
- **What a tree holds**: elements, texts, and instructions; comments with `options::keep_comments`; white space
  alone between elements with `options::keep_whitespace` — without it, text made only of white space is left out,
  so that a document indented for the eye gives the same tree as one written on a line (text with anything else
  in it is kept whole, its white space too). The pieces of one text — around a reference, a CDATA section, a
  comment left out — are one text node. The XML declaration and the DOCTYPE are not nodes; nor is anything
  outside the root: `parse` gives the root element, having read and checked the rest.
- **Line endings** are `"\n"` everywhere (XML 1.0 2.11). **Attribute values** are normalized (3.3.3): a literal
  tab, line feed or carriage return is a space; a character reference to one is that character.
- **`xml()`** is no node ([kind](../xml-kind.md)`::none`): what [child](child.md) gives when there is no such
  child, so that `doc.child("a").child("b").text()` needs no check at each step; its text is empty, it has no
  attributes and no children.
- **Changing**: `set`, `erase` and `push_back` copy the node they change (its attribute and children arrays; the
  children themselves are shared), so a `push_back` in a loop is quadratic: an element of many children is made
  with [xml::builder](../xml-builder/README.md).
- **Writing**: [to_string](to_string.md) escapes what must be escaped and writes a character XML cannot hold
  (a control, invalid UTF-8) as U+FFFD, as Go writes it. What `to_string` writes of a tree `parse` read, `parse`
  reads back as the same tree.
- **What is a program's mistake, not the input's, throws** `invalid_argument`: a name that is not a qualified
  name, a comment holding `--` or ending with `-`, an instruction's target `xml` or data holding `?>`, `set`,
  `erase` or `push_back` on a node that is not an element, `push_back(xml())`.
- A tree is walked with loops of its own, not recursion: `to_string`, `text()`, `==` and parsing take a tree
  deeper than any thread's stack.
- `parse` of a stream reads it on the thread that calls it; `async_parse` in a task gives the worker back while
  the stream waits. A document in memory never waits.
- A node holds tracked pointers: it lives where a `tracked_ptr` may (a stack, a managed object, a managed
  container).

### A program's types

A type with `describe(field_list&)` ([field_list](../field_list/README.md)) is mapped as Go maps a structure:

- A field is a child element of its name — the name as written (`dc:title`) or `{namespace}local` to match
  whatever the prefix; a field marked `attribute()` is an attribute, one marked `text()` the element's own text.
- A number, a boolean (`true`/`false`, read also as `1`/`0`), a string, an enum (by its `names()` or its value) or
  a type with `to_text`/`from_text` is text; numbers and booleans are read with the white space around them left
  out, strings as they are; a float's infinities and NaN are `INF`, `-INF`, `NaN`.
- A sequence or a set (`vector`, `list`, `set`, `array<T, N>`, the immutable ones) is the element repeated, once
  for each value; an optional or a `tracked_ptr` is an element (or an attribute) that may be absent; a type with
  `describe()` is an element holding its fields.
- Elements come in any order, those of a sequence among others; an element or an attribute the type has no field
  for is passed over. `required()` makes a missing one `errc::missing_field`, `omit_empty()` leaves an empty one
  out when written.
- What has no form in XML here — a map, a tuple, a variant, a list of lists, a `json` value, a record in an
  attribute — is `errc::unsupported_value` when written and `errc::type_mismatch` when read; a value nested deeper
  than 512 elements (a cycle of `tracked_ptr`) is `errc::unsupported_value`.
- The mapping goes through a tree: the element is read whole into nodes and the nodes mapped. Its error has the
  path inside (`/catalog/book[2]/price`, indexes from 1 as XPath counts, `@id` for an attribute, `text()` for the
  text) and the place where the element read began: the root for `parse<T>`, the element for
  `reader::read<T>`.

### From code written for Go

| With Go | With sgcl::encoding |
|---|---|
| `xml.Unmarshal` into structures standing for a document | `xml::parse(text)`: a tree, which Go has not |
| `xml.Unmarshal(b, &v)`, `xml.Marshal(v)` of a structure | `xml::parse<T>(text)`, `xml::stringify("name", v)`, the same `describe(field_list&)` as JSON's for the tags: `attr` is `attribute()`, `chardata` is `text()`; the root's name is given, not an `XMLName` field |
| `xml.Marshal`, `MarshalIndent` | `to_string()`, `to_string(xml::pretty)` of a tree |
| `xml.Name{Space, Local}` | `namespace_uri()`, `local_name()`; `"{space}local"` where a name is asked for |
| `Decoder.Strict = true`, the default | always strict: Go reads a second root, text around the root, an unbound prefix, two attributes of one name, invalid UTF-8 |
| `Decoder.Entity`, a map of entities | none: no entities but the five, as no DTD is read, by design |
| `Decoder.CharsetReader` | built in: UTF-16 and the 27 single byte encodings of txt |
| `Decoder.AutoClose`, `Strict = false` for input like HTML | none: XML only; HTML is another grammar |
| the tags `,innerxml`, `,any`, `,comment` | none: a field of type `xml` is not a kind of `field_list` yet |

### Conformance

The W3C XML Conformance Test Suite (xmlconf 2013-09-23, read from `~/Programming/oracles/xmlconf` when it is
there, not in the repository): of its 1965 tests of XML 1.0 fifth edition and Namespaces 1.0, 1536 are read as the
suite says; the other 429 are decided otherwise by design, each named with its reason in
`tests/encoding/xml_conformance_expected.h` — 317 documents whose error lies in the internal subset of their DTD
(skipped, not read), 29 whose error lies in an external DTD or entity (never loaded), 83 valid documents referring
to an entity their DTD declares. Of the 309 valid documents with a canonical form, 251 are written in it as the
suite writes them; the other 58 want an attribute default or type, or a notation, from their DTD.

Go's `encoding/xml` (`tools/xml_oracle.go` writes `tests/encoding/xml_go_tests.h`) gives the same tokens for the
1192 documents of the suite both read and for 32 documents named in `tools/xml_oracle.go`, which lists where the
two are compared otherwise (Go keeps line endings in comments, takes a UTF-8 byte order mark for text, keeps white
space in attribute values). 200,000 documents made by mutating those are read whole and through a stream in
pieces of 3 bytes, and every one read is written and read again as the same tree; where Go also reads one, its
tokens are Go's.

## Member types

| Type | Definition |
|---|---|
| `error` | [encoding::error](../error/README.md) |
| [kind](../xml-kind.md) | what a node is: none, an element, a text, a comment, an instruction |
| [attr](../xml-attr.md) | an attribute of an element: its name, value and namespace |
| [options](../xml-options.md) | what a parse or a reader accepts and keeps |
| [style](../xml-style.md) | how a node is written: the indentation and the XML declaration |
| [token](../xml-token/README.md) | one token of a document, as a reader gives it |
| [reader](../xml-reader/README.md) | the tokens of a document, from a string or a stream |
| [writer](../xml-writer/README.md) | XML written onto a stream, a call at a time |
| [builder](../xml-builder/README.md) | an element made a child at a time |

## Member objects

| Member | Description |
|---|---|
| `compact` | static: the [style](../xml-style.md) `{0, false}`, a node on one line without the XML declaration; the default of `to_string`, `stringify` and the writer |
| `pretty` | static: the style `{2, false}`, elements indented two spaces a level |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](xml.md) | no node, an empty element, or an element holding a text |

#### Making a node

| Function | Description |
|---|---|
| [text_node](text_node.md) | a text (static) |
| [comment](comment.md) | a comment (static) |
| [instruction](instruction.md) | a processing instruction (static) |

#### Reading

| Function | Description |
|---|---|
| [parse, async_parse](parse.md) | the root element of a document, or a value of a program's type (static) |
| [load, async_load](load.md) | the root element of a file, or a value of a program's type (static) |

#### Observers

| Function | Description |
|---|---|
| [type](type.md) | what the node is |
| [exists](exists.md) | checks whether the node is a node, not `xml()` |
| [is_element](is_element.md) | checks whether the node is an element |
| [is_text](is_text.md) | checks whether the node is a text |
| [name](name.md) | the name as written, or an instruction's target |
| [local_name](local_name.md) | the name without its prefix |
| [namespace_uri](namespace_uri.md) | the namespace of the element's name |

#### Lookup

| Function | Description |
|---|---|
| [attribute](attribute.md) | the value of an attribute |
| [attributes](attributes.md) | every attribute, in order |
| [children](children.md) | every node inside, or the elements of a name |
| [child](child.md) | the first element of a name, or `xml()` |
| [text](text.md) | the text of the node, or of the element's subtree joined |

#### New versions

| Function | Description |
|---|---|
| [set](set.md) | the element with an attribute set |
| [erase](erase.md) | the element without an attribute |
| [push_back](push_back.md) | the element with a node added as its last child |

#### A program's types

| Function | Description |
|---|---|
| [as](as.md) | this element as a value of a program's type |
| [from](from.md) | an element made of a value (static) |
| [stringify](stringify.md) | the text of an element made of a value (static) |

#### Writing

| Function | Description |
|---|---|
| [to_string](to_string.md) | the node as text |
| [save, async_save](save.md) | the node, or a value as an element, into a file |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | checks whether two nodes are the same tree, attributes in any order |

## Complexity

- Reading: linear in the length of the document.
- `set`, `erase`, `push_back`: linear in the attributes and the children of the node, which are copied; a
  `push_back` of n children one at a time is quadratic in n, the builder's linear.
- `child`, `attribute`: linear in the children, the attributes. `text`, `to_string`, `==`: linear in the size of
  the subtree.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = R"(<?xml version="1.0"?>
<catalog xmlns:dc="http://purl.org/dc/elements/1.1/">
  <book id="1"><dc:title>Lalka</dc:title><price>39.90</price></book>
  <book id="2"><dc:title>Solaris &amp; more</dc:title></book>
</catalog>)";
    auto doc = encoding::xml::parse(text);
    if (!doc) {
        println(doc.error().message());
        return 1;
    }
    for (auto book : doc->children("book")) {
        string id = book.attribute("id", "?");
        string title = book.child("{http://purl.org/dc/elements/1.1/}title").text();
        string price = book.child("price").text();  // "" when there is none
        println("{}: {} [{}]", id, title, price);
    }

    // a change is a new tree; the one read stays as it was
    encoding::xml added = doc->push_back(
        encoding::xml("book").set("id", "3").push_back(encoding::xml("dc:title", "Diuna")));
    println(added.to_string(encoding::xml::pretty));
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
```

## See also

- [xml::reader](../xml-reader/README.md): a document a token at a time
- [xml::writer](../xml-writer/README.md): XML onto a stream
- [xml::builder](../xml-builder/README.md): an element made a child at a time
- [field_list](../field_list/README.md): a program's type described by its fields
- [error](../error/README.md), [errc](../errc.md): what a reading or a mapping failed with
- [json](../json/README.md): the same kind of value for JSON
- [txt::encoding](../../txt/encoding.md): the single byte encodings a declaration names
- [sgcl::encoding](../README.md)
