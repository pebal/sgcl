[sgcl](../../README.md) › [encoding](../README.md)

# sgcl::encoding::yaml

```cpp
#include "sgcl/encoding/yaml.h"   // or "sgcl/encoding.h"

namespace sgcl::encoding {
    class yaml;
}
```

**Requires [rooted](../../core/rooted/README.md) outside a stack or a managed object.**

`sgcl::encoding::yaml` is one node of [YAML 1.2.2](https://yaml.org/spec/1.2.2/), immutable, as
[json](../json/README.md) is one JSON value: 24 bytes, a pointer, a word and a kind, shared by copying. A scalar keeps
its text as it was written and the kind the core schema reads it as — null, a boolean, an integer, a float, a string
— a sequence its elements, a mapping its members, keys of any kind. [parse](parse.md) reads one document,
[parse_all](parse_all.md) every document of a stream, and [to_string](to_string.md) writes a document in block style.
An alias is the node of its anchor, shared, not copied. Go's standard library has no YAML; what a Go program reads
with `gopkg.in/yaml.v3` into a `yaml.Node` or a `map[string]any`, this reads into a node.

## Rules

- **What is read**: every construct of YAML 1.2 — directives (`%YAML`, `%TAG`), documents (`---`, `...`, bare),
  comments, block sequences and mappings (compact, indentless, explicit `?` keys), flow collections (with single
  pairs in sequences), the five scalar styles (plain over several lines, single- and double-quoted with every escape,
  literal and folded with their chomping and indentation indicators), anchors, aliases and tags (`!local`,
  `!!core`, `!<verbatim>`, the handles of `%TAG`). Errors have their line and column.
- **The core schema** (§10.3): a plain scalar is null (`null`, `~`, nothing), a boolean (`true`, `false` in three
  cases), an integer (decimal, `0o` octal, `0x` hexadecimal), a float (with `.inf` and `.nan`), else a string; a
  quoted one is always a string. YAML 1.1's `yes`, `no`, `on`, sexagesimals and `<<` merge keys are strings and keys
  here. A core tag forces the kind (`!!str 12` is a string, `!!int "12"` an integer) and is no tag of the node; an
  application's tag (`!Ref`, `!!binary`) is kept, [tag](tag.md), and the plain text under it still read by the core
  schema.
- **What is refused**: a key given twice in one mapping (YAML says keys are unique; `allow_duplicate_keys` lets the
  last win), a character YAML does not allow (a control character but tab and the line breaks, a C1 control but
  NEL), invalid UTF-8, a second document where [parse](parse.md) asks for one.
- **The limits** ([yaml::options](../yaml-options.md)): collections nested deeper than `max_depth` (512); more than
  `max_alias_nodes` (2^20) nodes reached through aliases — the billion laughs, nine anchors of nine aliases each,
  stopped at once.
- **What is written**: block style, two spaces a level; a string plain when it reads back as the same string, a
  literal block for text of several lines, double quotes with escapes otherwise; `[]` and `{}` for empty
  collections; a key that is a collection, or one past 1000 characters, after `? `; an application's tag before its
  node; a node shared through an alias written at every place. What is read keeps its text, so `0o14` is written
  `0o14` and `~` is written `~`. Comments and styles are not kept.
- **The oracle**: libyaml reads the examples of the spec's chapter 2, the edge cases of every construct, and 600
  documents this writer wrote of random values as the same graphs (where YAML 1.1 and 1.2 differ — `?a` and `a:b` in
  flow, an anchor's characters, the uniqueness of keys — the tests name the case and the reason); the writer's text
  reads back to an equal node and writes the same text.

## Member types

| Type | Definition |
|---|---|
| `error` | [encoding::error](../error/README.md) |
| `kind` | the kind of a node: [yaml::kind](../yaml-kind.md) |
| `member` | a key and its value: [yaml::member](../yaml-member.md) |
| `options` | what a parse accepts: [yaml::options](../yaml-options.md) |

## Member functions

| Function | Description |
|---|---|
| [(constructor)](yaml.md) | null, a boolean, an integer, a float, a string |
| [parse, async_parse](parse.md) | one document of a text or a stream (static) |
| [parse_all](parse_all.md) | every document of a text (static) |
| [to_string](to_string.md) | the node as a document |
| [load, async_load](load.md) | the document of a file (static) |
| [save, async_save](save.md) | the node into a file |

#### Making one

| Function | Description |
|---|---|
| [sequence](sequence.md) | a sequence (static) |
| [mapping](mapping.md) | a mapping (static) |
| [tagged](tagged.md) | a node under an application's tag (static) |
| [from_json](from_json.md) | a JSON value as YAML (static) |
| [to_json](to_json.md) | the node as JSON |

#### Observers

| Function | Description |
|---|---|
| [type](type.md) | the kind |
| [is_null](is_null.md) | whether it is null |
| [is_bool](is_bool.md) | whether it is a boolean |
| [is_number](is_number.md) | whether it is an integer or a float |
| [is_integer](is_integer.md) | whether it is an integer |
| [is_string](is_string.md) | whether it is a string |
| [is_sequence](is_sequence.md) | whether it is a sequence |
| [is_mapping](is_mapping.md) | whether it is a mapping |
| [as_bool](as_bool.md) | a boolean |
| [as_int](as_int.md) | an integer within `int64_t` |
| [as_uint](as_uint.md) | a non-negative integer |
| [as_double](as_double.md) | a float, or an integer rounded |
| [as_string](as_string.md) | a string |
| [text](text.md) | a scalar as written |
| [tag](tag.md) | an application's tag |

#### Element access

| Function | Description |
|---|---|
| [operator[]](operator_at.md) | an element by its index, a value by its key |
| [contains](contains.md) | whether a mapping has a key |
| [size](size.md) | the elements or members |
| [empty](empty.md) | whether there is none |
| [elements](elements.md) | a sequence's elements |
| [members](members.md) | a mapping's members |

#### New versions

| Function | Description |
|---|---|
| [set](set.md) | with a key set, or an element |
| [erase](erase.md) | without a key, or an element |
| [push_back](push_back.md) | with an element added |

#### Conversions

| Function | Description |
|---|---|
| [hash](hash.md) | a hash, equal for equal nodes |

## Non-member functions

| Function | Description |
|---|---|
| [operator==](operator_cmp.md) | deep equality by the values read |

## Example

```cpp
#include "sgcl/encoding.h"
#include "sgcl/io.h"

using namespace sgcl;

int main() {
    encoding::yaml config = encoding::yaml::parse(R"(
server:
  host: example.com
  port: 8080
  tls: true
paths: [/a, /b]
)").value();
    println("{}:{}", config["server"]["host"].as_string("?"), config["server"]["port"].as_int(80));
    for (const auto& path : config["paths"].elements()) {
        println(path.as_string("?"));
    }
    print(config.set("debug", true).to_string());
}
```

Output:

```text
example.com:8080
/a
/b
server:
  host: example.com
  port: 8080
  tls: true
paths:
  - /a
  - /b
debug: true
```

## See also

- [json](../json/README.md): the JSON value; [to_json](to_json.md), [from_json](from_json.md)
- [cbor](../cbor/README.md)
- [sgcl::encoding](../README.md)
