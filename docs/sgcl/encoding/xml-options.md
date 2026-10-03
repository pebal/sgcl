[sgcl](../README.md) › [encoding](README.md) › [xml](xml.md)

# sgcl::encoding::xml::options

```cpp
#include "sgcl/encoding/xml.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class xml {
    public:
        struct options {
            uint32_t max_depth = 512;
            size_t max_token_size = size_t(16) << 20;
            bool keep_comments = false;
            bool keep_whitespace = false;
        };
    };
}
```

`sgcl::encoding::xml::options` is what [parse](xml/parse.md), [load](xml/load.md) and a
[reader](xml-reader.md) accept and keep: the limits for a document from outside, and the nodes a tree leaves out
by default. The limits are the only things that grow with the input but the tree itself: a document of any size
is read with a reader a node at a time.

## Member objects

| Member | Description |
|---|---|
| `max_depth` | elements nested deeper are `errc::depth_limit`; 512 by default |
| `max_token_size` | a tag, a text or a comment of a stream not ended within this many bytes is `errc::out_of_range`: the buffer of a stream grows to hold a token and no more; 16 MB by default. A document in memory is held whole already and has no such limit |
| `keep_comments` | comments in a tree and in the reader's `read()`; the reader's `next()` gives them always; `false` by default |
| `keep_whitespace` | text of white space alone between elements, the same; without it such text is left out, so that a document indented for the eye gives the same tree as one written on a line; `false` by default |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    string text = "<list>\n  <!-- one -->\n  <i>1</i>\n</list>";
    println(encoding::xml::parse(text)->to_string());

    encoding::xml::options all;
    all.keep_comments = true;
    all.keep_whitespace = true;
    println(encoding::xml::parse(text, all)->to_string());

    encoding::xml::options shallow;
    shallow.max_depth = 1;
    println(encoding::xml::parse(text, shallow).error().message());
}
```

Output:

```text
<list><i>1</i></list>
<list>
  <!-- one -->
  <i>1</i>
</list>
3:3 /list: elements nested deeper than options.max_depth (1)
```

## See also

- [parse](xml/parse.md), [reader](xml-reader.md): what takes the options
- [sgcl::encoding::xml](xml.md)
