# sgcl::encoding::field_list

```cpp
#include "sgcl/encoding/fields.h"   // or "sgcl/encoding/encoding.h"

namespace sgcl::encoding {
    class field_list;   // the fields of an object, as its describe() names them
    class field;        // the options of one field, chained
}
```

A type of the program described by its fields, once for every format of the module: a method `void describe(field_list& f)` that names each field — `f.add("name", name)` — and JSON reads and writes it ([`json::parse<T>`](json.md), `json::stringify`, [`json::reader::read<T>`](json_reader.md), [`json::writer::value`](json_writer.md)), XML too ([`xml::parse<T>`](xml.md), `xml::stringify`, [`xml::reader::read<T>`](xml-reader.md), [`xml::writer::value`](xml-writer.md): a field is a child element, `attribute()` an attribute, `text()` the text), and CSV ([`csv::reader::read<T>`](csv.md)). What the tags of a Go structure are (`json:"name,omitempty"`), written as code: no macro, no pointer to a member, no template on the side of the type; the type stays an aggregate.

## Rules

- **One method, both ways, every format.** `describe` names the fields; reading fills them, writing reads them. A base class is described by calling its `describe` first (`base::describe(f);`). A field may be private: the method sees it.
- **A type of another library** is described by a free function `void describe(field_list& f, lib::point& p)` in its namespace or in `sgcl`, which argument-dependent lookup finds.
- **Two doors for a type fields cannot describe.** `json to_json() const` with `static optional<T> from_json(const json&)` gives a JSON of its own (`[x, y]` for a point); `string to_text() const` with `static optional<T> from_text(const string&)` gives one text for every format — a JSON string, a CSV field, an XML attribute, a key of a map (a uuid, a date). `from_json` or `from_text` returning `nullopt` is `type_mismatch` at the place of the value.
- **The kinds a field may have**, with no description: `bool`; every integer type but the characters, range-checked when read; `float` and `double`, each rounded once from the decimal (a float is never a double rounded again) and written with its own shortest digits; `string` and `std::string`; `json` (the value as it is, Go's `RawMessage`); `optional<T>` (null or the value); `tracked_ptr<T>` (null, or a new object when read); `vector`, `deque`, `list`, `dynamic_array`, `immutable::vector`, `immutable::list` and any range with `push_back` (an array); `array<T, N>`, `std::array`, `T[N]` (an array of exactly N); `set`, `sorted_set`, `ordered_set`, `immutable::set` (an array); `map`, `sorted_map`, `ordered_map`, `immutable::map` keyed by a string, an integer or a type with `to_text` (an object); `pair`, `tuple` (an array of their elements); `variant` with `tagged` (below); an enum (its integer, or one of `names`); another described type (an object). A container of `std` holds only what holds no tracked pointer — numbers, enums, `std::string` — which the compiler checks; the library's containers hold anything.
- **Hash maps and hash sets are written sorted** (by the text of the key, of the element), as Go sorts the keys of a map: the text of a value does not depend on the key of the hash. `style::sort_keys = false` keeps their own order. A sorted or ordered container is written in its order.
- **Reading an object**: the fields by their names, case-sensitive; a key no field has is skipped (`options::reject_unknown_fields`: `unknown_field`); a key given twice is `duplicate_key`; a field that is not there keeps its value, unless it is `required()`: `missing_field`. JSON's null in a field that is not optional or a pointer puts the default value there, as Go's v2 does.
- **An integer field takes a number whose value is an integer**: `1.0` and `1e2` are 1 and 100 (Go's v2 refuses both), `1.5` is `type_mismatch`, `300` into a `uint8_t` is `out_of_range`.
- **A variant is an object with a tag**: `f.add("shape", shape).tagged("type", {"circle", "square"})` writes `{"type": "circle", "r": 1}` — the tag, then the fields of the alternative, which is a described type — and reads the tag wherever it is among the members. A variant without `tagged` is `unsupported_value`.
- **The errors say where**: the path of the value that failed as a JSON Pointer (`/users/3/age`), with the line and the column in a text. A cycle of pointers is found by the depth when written (512, `unsupported_value`); NaN is `unsupported_value` with its path.
- **Nothing is kept between two objects.** `describe` is called on the object each time it is read or written, and the list it fills lives on the stack of the call; the operations of each type are one constant table, which holds no tracked pointer. The type needs a default constructor to be read (an element of a container, a new object behind a pointer).

## Members

```cpp
class field_list {
public:
    template<class T>
    field& add(const char* name, T& value);   // the name a literal: it is read after describe returns
    size_t size() const noexcept;
};

class field {
public:
    field& required() noexcept;       // missing when read: missing_field
    field& omit_empty() noexcept;     // not written when empty: 0, false, "", an empty container, null, a default value
    field& quoted() noexcept;         // a number or a boolean as a string: "12" (Go's ,string)
    field& attribute() noexcept;      // XML: an attribute of the element
    field& text() noexcept;           // XML: the text of the element
    field& names(std::initializer_list<const char*> names);                   // an enum as a name: names[value]
    field& tagged(const char* key, std::initializer_list<const char*> names);  // a variant as an object with a tag
};
```

## Example

```cpp
#include "sgcl/sgcl.h"

using namespace sgcl;
using encoding::field_list;

enum class role { reader, writer, admin };

struct address {
    string city;
    string street;

    void describe(field_list& f) {
        f.add("city", city);
        f.add("street", street);
    }
};

struct user {
    string name;
    int age = 0;
    vector<string> tags;
    optional<address> home;
    tracked_ptr<user> manager;
    role access = role::reader;
    map<string, int> scores;

    void describe(field_list& f) {
        f.add("name", name).required();
        f.add("age", age).omit_empty();
        f.add("tags", tags);
        f.add("home", home);
        f.add("manager", manager);
        f.add("access", access).names({"reader", "writer", "admin"});
        f.add("scores", scores).omit_empty();
    }
};

int main() {
    auto u = encoding::json::parse<user>(R"({"name": "Ala", "age": 30, "tags": ["a"], "access": "admin",
        "manager": {"name": "Ola", "home": {"city": "Kraków", "street": "Długa"}}})");
    io::stdout.write(u->name + " reports to " + u->manager->name + " in " + u->manager->home->city + "\n");

    u->scores = {{"go", 3}, {"cpp", 5}};
    io::stdout.write(encoding::json::stringify(*u).value() + "\n");

    auto wrong = encoding::json::parse<user>(R"({"name": "Ala", "manager": {"name": "Ola", "age": "old"}})");
    io::stdout.write(wrong.error().message() + "\n");
    auto missing = encoding::json::parse<user>(R"({"age": 3})");
    io::stdout.write(missing.error().message() + "\n");
}
```

Output:

```text
Ala reports to Ola in Kraków
{"name":"Ala","age":30,"tags":["a"],"home":null,"manager":{"name":"Ola","tags":[],"home":{"city":"Kraków","street":"Długa"},"manager":null,"access":"reader"},"access":"admin","scores":{"cpp":5,"go":3}}
1:51 /manager/age: expected an integer, found a string
1:10 /name: missing field
```

## For a format: the mechanism

A format reads a described value through `detail::FieldAccess::fields(list)`: a `detail::FieldInfo` for each field — the `name`, the `address` of the member, `ops` (the `detail::ValueOps` of its type), the `flags` (`Required`, `OmitEmpty`, `AsString`, `Attribute`, `Text`), and the names given by `names` or `tagged` (`FieldAccess::name`, `FieldAccess::index_of`). `ValueOps::kind` is one of `detail::ValueKind` — `boolean`, `signed_integer`, `unsigned_integer`, `floating`, `string`, `text`, `enumeration`, `optional`, `pointer`, `sequence`, `fixed`, `set`, `map`, `tuple`, `record`, `variant`, `json`, `custom_json` — and the table holds the functions that kind needs: `get_*`/`set_*` of a scalar, `set_literal` (a number from its decimal text: exactly, or rounded once), `number_text`, `get_text`/`set_text`, `has_value`/`value`/`emplace`/`reset` of an optional or a pointer, `count`/`for_each`/`read_elements` of a container, `element`/`element_of` of a fixed array or a tuple, `for_each_entry`/`read_entries` of a map (the key as text), `describe` of a record, `index`/`alternative`/`emplace_alternative` of a variant, `is_empty`, `clear`. A format is written once against the kinds, never against the types: JSON's reader and writer in [`json.h`](json.md) are the example.

## SGCL and Go

| Go | SGCL | note |
|---|---|---|
| a struct with tags `json:"name"` | `void describe(field_list& f) { f.add("name", name); }` | one method for every format and both ways |
| `json:",omitempty"`, v2 `omitzero` | `.omit_empty()` | |
| `json:",string"` | `.quoted()` | |
| — (v2: no required) | `.required()` | `missing_field` with the path |
| `xml:",attr"`, `xml:",chardata"` | `.attribute()`, `.text()` | |
| `Marshaler`, `Unmarshaler` | `to_json()`, `from_json(const json&)` | |
| `TextMarshaler`, `TextUnmarshaler` | `to_text()`, `from_text(const string&)` | a JSON string, a CSV field, an XML attribute, a map's key |
| interfaces and `any` in a struct | `variant` with `.tagged("type", {...})`; a field of type `json` | |
| `UnmarshalTypeError.Field` | `error.path()` | a JSON Pointer, and the line and the column |

## See also

[`json`](json.md): `parse<T>`, `stringify`, `as<T>`; [`json::reader`](json_reader.md), [`json::writer`](json_writer.md); [`error`](error.md).
