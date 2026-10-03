[sgcl](../../README.md) › [encoding](../README.md) › [xml](README.md)

# sgcl::encoding::xml::text

```cpp
string text() const;
```

The text of the node: of a text node and a comment their own, of an instruction its data; of an element every text
inside it and inside its descendants, joined in the order of the document, without the comments and the
instructions; of `xml()` an empty string. A tree of any depth is walked with a loop of its own, not recursion.

## Parameters

None.

## Return value

The text, or an empty string.

## Complexity

Constant for a node that is not an element; linear in the size of the subtree of an element.

## Exceptions

`length_error` when the text joined would pass `string::max_size()`.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::xml::options o;
    o.keep_comments = true;
    auto p = encoding::xml::parse("<p>Hello <b>dear <i>old</i></b> world<!--!-->.</p>", o).value();
    println(p.text());
    println(p.child("b").text());
    println("{} [{}]", p.children()[3].text(), encoding::xml().text());
}
```

Output:

```text
Hello dear old world.
dear old
! []
```

## See also

- [child](child.md): the first element of a name, whose text is asked often
- [text_node](text_node.md): a text made by the program
- [sgcl::encoding::xml](README.md)
