[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml.md) › [reader](../xml-reader.md)

# sgcl::encoding::xml::reader::reader

```cpp
explicit reader(const string& text) noexcept;               // (1)
reader(const string& text, const options& o) noexcept;      // (2)
explicit reader(const io::reader& in) noexcept;             // (3)
reader(const io::reader& in, const options& o) noexcept;    // (4)
reader(reader&& other) = default;                           // (5)
reader(const reader&) = delete;                             // (6)
```

Constructs a reader. Nothing is read until the first call that asks for a token.

1. A reader of the document in `text`, with the default [options](../xml-options.md). The reader holds the string,
   which is shared, not copied.
2. The same with the options `o`.
3. A reader of the document a stream brings, read as the tokens are asked for, with the default options.
4. The same with the options `o`.
5. Takes the reading of `other` over, where it is; `other` is left to be destroyed or assigned to.
6. A reader is not copied: two readers would take turns at one stream.

## Parameters

| Parameter | Description |
|---|---|
| `text` | the document, in UTF-8, UTF-16 or an encoding its declaration names |
| `in` | the stream that brings the document |
| `o` | what is accepted and kept: the limits, the comments, the white space |
| `other` | the reader taken over |

## Complexity

Constant.

## Exceptions

None.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::options o;
    o.keep_comments = true;
    encoding::xml::reader r("<log><!-- start --><line>up</line></log>", o);
    r.next();
    while (auto node = r.read()) {
        println(node->to_string());
    }

    io::write_file("log.xml", "<log><line>down</line></log>").value();
    encoding::xml::reader from_file(io::open("log.xml").value());
    encoding::xml::reader moved = std::move(from_file);
    moved.next();
    println(moved.read()->text());
}
```

Output:

```text
<!-- start -->
<line>up</line>
down
```

## See also

- [operator=](operator_assign.md): takes another reader over
- [xml::parse](../xml/parse.md): a document read whole
- [sgcl::encoding::xml::reader](../xml-reader.md)
