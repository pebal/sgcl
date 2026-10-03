[sgcl](../../README.md) › [encoding](../README.md) › [xml](../xml/README.md) › [writer](README.md)

# sgcl::encoding::xml::writer::value

```cpp
template<class T> writer& value(const string& name, const T& v);
```

Writes a value of a program's type as the element `name`, mapped as
[A program's types](../xml/README.md#a-programs-types) says: [xml::from](../xml/from.md), then [node](node.md). Go's
`Encoder.Encode(v)` and `EncodeElement(v, start)`. A value with no form in XML (a map, a variant…) is a mistake
(`errc::unsupported_value`, with its path), kept as the others are and given by [flush](flush.md). A stream of
values is written a value at a time, in the memory of one.

## Parameters

| Parameter | Description |
|---|---|
| `name` | the name of the element |
| `v` | the value |

## Return value

`*this`.

## Complexity

Linear in the size of the value.

## Exceptions

What the `describe` of `T` throws.

## Example

```cpp
#include "sgcl/core.h"
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

struct reading {
    string sensor;
    double celsius = 0;

    void describe(encoding::field_list& f) {
        f.add("sensor", sensor).attribute();
        f.add("celsius", celsius).text();
    }
};

int main() {
    encoding::xml::writer w(io::stdout, encoding::xml::pretty);
    w.start("log");
    for (int i : range(3)) {
        w.value("reading", reading{"t" + to_string(i), 20.5 + i});
    }
    w.end();
    w.flush().value();
    println();
}
```

Output:

```text
<log>
  <reading sensor="t0">20.5</reading>
  <reading sensor="t1">21.5</reading>
  <reading sensor="t2">22.5</reading>
</log>
```

## See also

- [node](node.md): a node of a tree, whole
- [reader::read](../xml-reader/read.md): `read<T>`, the way back
- [xml::stringify](../xml/stringify.md): the text of a value without a stream
- [sgcl::encoding::xml::writer](README.md)
