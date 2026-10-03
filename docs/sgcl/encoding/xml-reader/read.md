[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml/README.md) › [reader](README.md)

# sgcl::encoding::xml::reader::read, async_read

```cpp
optional<xml> read();                                                // (1)
async::task<optional<xml>> async_read() noexcept;                    // (2)
template<class T> optional<T> read();                                // (3)
template<class T> async::task<optional<T>> async_read() noexcept;    // (4)
```

The next node whole, read from where the reader is.

1. The next node as a [tree](../xml/README.md): an element with everything inside it, a text (the pieces of one text
   joined), a comment or an instruction as the [options](../xml-options.md) keep them. Left out, as a tree leaves
   them out, are comments without `keep_comments`, white space alone without `keep_whitespace`, and the XML and
   DOCTYPE declarations.
2. (1) in a task: `co_await r.async_read()` gives the worker back while the stream waits.
3. The next element as a value of a program's type `T`, mapped as [A program's types](../xml/README.md#a-programs-types)
   says: Go's `DecodeElement`. An element that is not a `T`, or text where an element was expected, stops the
   reader: [last_error](last_error.md) has the path inside the element and the offset of its start. A document of
   any length is read a value at a time in the memory of one element.
4. (3) in a task.

At the end tag of the element the reader is inside, `read` gives `nullopt` and leaves the end tag for
[next](next.md): `while (auto child = r.read())` goes over the children of the element whose start was the last
token. `nullopt` at the end of the document and on an error as well.

## Parameters

None.

## Return value

The node (1–2) or the value (3–4); `nullopt` at the end tag of the element around it, at the end of the document,
on an error.

## Complexity

Linear in the length of the node read; (3–4) and in the size of the tree mapped.

## Exceptions

- (1) What [next](next.md) throws; `length_error` when the text gathered of one text node would pass
  `string::max_size()`.
- (3) What (1) throws, and what the default constructor of `T` and its `describe` throw.
- (2, 4) None: what (1) and (3) throw, the task's `co_await` or `wait()` throws again.

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

struct entry {
    int id = 0;
    string title;

    void describe(encoding::field_list& f) {
        f.add("id", id).attribute();
        f.add("title", title);
    }
};

int main() {
    encoding::xml::reader r(R"(<feed>
  <entry id="1"><title>First</title></entry>
  <entry id="2"><title>Second</title></entry>
  <entry id="x"><title>Third</title></entry>
</feed>)");
    r.next();
    while (auto e = r.read<entry>()) {
        println("{} {}", e->id, e->title);
    }
    println(r.last_error()->message());

    encoding::xml::reader tree("<p>a <b>b</b> c</p>");
    tree.next();
    while (auto node = tree.read()) {
        println("[{}]", node->to_string());
    }
    println(tree.next()->is_end("p"));
}
```

Output:

```text
1 First
2 Second
offset 100 /entry/@id: expected an integer, found "x"
[a ]
[<b>b</b>]
[ c]
true
```

## See also

- [skip](skip.md): passes over the node `read` would give
- [peek](peek.md): the token in front, looked at first
- [xml::as](../xml/as.md): a node of a tree as a value
- [sgcl::encoding::xml::reader](README.md)
