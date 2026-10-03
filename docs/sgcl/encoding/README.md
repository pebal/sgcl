[sgcl](../README.md) › encoding

# sgcl::encoding

```cpp
#include "sgcl/encoding.h"   // namespace sgcl::encoding
```

What Go has in `encoding/...`: the formats data is written in when it leaves a program and read in when it comes
back. The byte codecs — [base64](base64.md), [base32](base32.md), [hex](hex.md), [ascii85](ascii85.md),
[pem](pem.md) and the binary numbers ([big_endian](big_endian.md), [little_endian](little_endian.md),
[varint](varint.md)) — [json](json.md) (an immutable value, a reader of tokens and values, a writer),
[csv](csv.md) (a reader, its rows, a writer), [xml](xml.md) (a tree that never changes, a reader of tokens, a
writer), and the description of a program's own types by their fields, [field_list](field_list.md), which JSON,
CSV and XML read and write alike. The module depends on [core](../core/README.md), [txt](../txt/README.md) (the
character encodings an XML declaration names), [async](../async/README.md) and [io](../io/README.md) (a codec is a
stream as well as a function); the index of the whole interface is [the modules](../README.md).

The module is the namespace `sgcl::encoding`, as every module but core is a namespace of its own. Inside it every
format is one type named as the format is named, and everything that belongs to a format is a member of it:
`encoding::base64::standard`, `encoding::base64::encoder`, `encoding::pem::parse`, `encoding::json::reader`.
`encoding::base64::standard.encode(b)` reads as Go's `base64.StdEncoding.EncodeToString(b)`. A codec that has
something to choose — an alphabet, the padding, strict or lenient — is a value with `encode` and `decode` as its
methods, and the choices Go names are constants of it (`encoding::base64::url`, `encoding::base32::hex`); a codec
with nothing to choose (`hex`, `ascii85`, `varint`) has the same methods as static ones. So every codec is used
the same way, and a codec of one's own — base64 with the alphabet of `crypt(3)` — is one line.

A reading refuses what it cannot tell apart, and says where. Nothing in the module throws on its input: a function
that reads a whole text returns an [expected](../core/expected.md)`<T, encoding::error>`, a stream that reads one
fails its read with an `io::error` and keeps the [error](error.md) for the asking. One type of error serves every
format, with the code, the byte of the input it was found at, the line and the column where the format has lines,
and the path inside the structure where it has one.

## The rules

1. **One error type, under every format's name.** The errors of the module are [error](error.md) and
   [errc](errc.md), and each format names the same type `error` (`encoding::base64::error`,
   `encoding::pem::error`, `encoding::json::error`). The offset is where the input stops being the start of
   something valid, so base64's `QQ=x` fails at the `x` and a cut text at its end; `message()` reads
   `offset 17: invalid character '*'` or `3:10: END B does not match BEGIN A`. The line and the column are counted
   from the text after the fact, when something failed: a reading that succeeds never counts a line ending.
2. **What throws is a mistake in the program, not in its input:** an alphabet with a repeated character
   (`invalid_argument`, an error at compile time in a constant), a caller's buffer smaller than the size the codec
   asked for (`length_error`), a PEM type that is not a label, a name that is not one of XML, a CSV separator that
   cannot be one (`invalid_argument`), an element pushed into a JSON builder of an object or a member set in one of
   an array (`logic_error`), a field of a CSV row asked by `at` past the row's end (`out_of_range`). A text the program itself writes may be constructed and throws
   [bad_expected_access](../core/bad_expected_access.md) when it is wrong; a text from outside is parsed. A
   result longer than a [string](../core/string.md) holds (4 GiB) is `length_error`. Each page's Exceptions say
   which.
3. **Strict by default.** base64 and base32 refuse a character outside the alphabet, a line ending included
   (RFC 4648 section 3.3), and bits past the data in the last character (section 3.5), which would let two
   different texts decode to the same bytes — what a signature, a key or a token must not allow. `lenient()` takes
   both, for MIME and the encoders that wrap their lines; it is Go's default, so a text Go reads, `lenient()`
   reads. PEM decodes its base64 strictly too. An ascii85 group worth more than 32 bits is refused where Go takes
   it modulo 2^32. XML is read strictly always, with no DTD.
4. **Where the module and Go differ in what they accept,** it is written on the codec's page and held to in the
   tests by name: a line ending in a strict decoding, bits past the data, what follows base32's padding, the byte
   `0xFF` that Go's base32 without padding takes for padding, an ascii85 group past 32 bits, a tenth varint byte
   with its high bit set, bits past the data in a PEM block, a malformed PEM block.
5. **Memory.** A codec is plain data — the characters of its alphabet and a table back — and holds no tracked
   pointer, so the constants are constants and a codec lives anywhere. What a codec returns is the library's: a
   [string](../core/string.md) for text and a [vector](../core/vector.md)`<byte>` for bytes; the input is a
   `slice<const byte>` (a vector, a string's bytes, a raw buffer) or a `const string&`. `encode_to` and
   `decode_to` write into the caller's buffer and allocate nothing. A stream codec is a handle, one tracked word to
   a managed object holding its block, 8 KB of `array<byte, N>` as io's buffers are, and the stream under it; it
   lives where a `tracked_ptr` may ([the rules of core](../core/README.md#the-rules), 1). A [json](json.md) and an
   [xml](xml.md) node hold tracked words too, and live there as well.
6. **A program's own types** are read and written through one description, a method
   `void describe(field_list& f)` that names each field ([field_list](field_list.md)): `json::parse<T>`,
   `json::stringify`, `csv::reader::read<T>`, `xml::parse<T>` and the rest take any type that has one, and so does
   [slog](../slog/README.md) when it writes a value of the program's. Go's struct
   tags are that description; Go's `binary.Read` and `binary.Write` of structures and `encoding/gob` (a
   serialization of object graphs) have no counterpart yet, and ASN.1 is left to `crypto`.
7. **Its own implementation, tested against Go.** The byte codecs are tested in `tests/encoding` against Go's
   packages as the oracle (`tools/encoding_oracle.go` writes `tests/encoding/encoding_tests.h`): the vectors of
   RFC 4648 section 10; every length of input from 0 to 300 through the ten codecs; every byte from 0 to 255 at
   every position of a valid text of each codec, accepted or refused and where; varints both ways and every way a
   read can fail; blocks Go writes and texts Go reads for PEM, and the fourteen examples of RFC 7468 itself. The
   streams are fed and read in pieces of 1, 2, 3 and 7 bytes and must come out as the whole text does; the sizes
   are held at the edge of `size_t`. JSON (`tests/encoding/json_*.cpp`) is tested against Go's v2
   (`tools/json_oracle.go`): a million doubles and a million floats written, half a million literals read both
   ways, every file of JSONTestSuite decided as v2 decides it with its tokens and its value written back; the
   reader fed 1, 2, 3 and 7 bytes at a time; differential fuzzing against Go (`tools/json_fuzz.go`, run with
   `SGCL_JSON_FUZZ`). Typed JSON is held to Go's `Marshal` of the same structure; JSON's files, `load` and
   `save`, are tested in `tests/encoding/files.cpp`. CSV (`tests/encoding/csv.cpp`) is
   tested against Go's `encoding/csv` (`tools/csv_oracle.go`): named texts with their fields' places and errors, a
   hundred thousand random texts, whole and in pieces, and the text Go's writer writes. XML is tested against the
   W3C XML Conformance Test Suite (read from `~/Programming/oracles/xmlconf` when it is there; every test decided
   otherwise by design is named with its reason) and against Go's `encoding/xml` (`tools/xml_oracle.go` writes
   `tests/encoding/xml_go_tests.h`): the tokens of the suite's documents and of named ones, and, with `-fuzz`, of
   mutated ones; a stream in pieces of every small size gives what the whole document gives, errors and their
   places included; the attacks (billion laughs, XXE, the quadratic blowup), the limits, and a token fed a byte at
   a time read in linear time.

## Functions

| Function | Header | Description |
|---|---|---|
| [encoding_category](encoding_category.md) | `error.h` | the `std::error_category` of the module's codes, named `"encoding"` |
| [make_error_code](make_error_code.md) | `error.h` | the `error_code` of an [errc](errc.md), found by the standard library through its namespace |

## Classes

| Class | Header | Description |
|---|---|---|
| [ascii85](ascii85.md) | `ascii85.h` | Ascii85 as `btoa` writes it and Go reads it, four bytes as five characters, `z` for four zeros: `encode`, `decode`, the streams; a group past 32 bits refused |
| [ascii85::decoder](ascii85-decoder.md) | `ascii85.h` | a reader of the bytes another reader's Ascii85 decodes to |
| [ascii85::encoder](ascii85-encoder.md) | `ascii85.h` | a writer that writes the Ascii85 of what it is given to another writer |
| [base32](base32.md) | `base32.h` | base32 of RFC 4648, five bytes as eight characters: `standard`, `hex`, an alphabet of one's own, strict or `lenient()`, the members of base64 |
| [base32::decoder](base32-decoder.md) | `base32.h` | a reader of the bytes another reader's base32 decodes to |
| [base32::encoder](base32-encoder.md) | `base32.h` | a writer that writes the base32 of what it is given to another writer |
| [base64](base64.md) | `base64.h` | base64 of RFC 4648, three bytes as four characters: `standard`, `url`, `raw_standard`, `raw_url`, an alphabet of one's own, strict or `lenient()`; `encode`, `decode`, into the caller's buffer, as streams |
| [base64::decoder](base64-decoder.md) | `base64.h` | a reader of the bytes another reader's base64 decodes to |
| [base64::encoder](base64-encoder.md) | `base64.h` | a writer that writes the base64 of what it is given to another writer |
| [big_endian](big_endian.md) | `binary.h` | numbers of 16, 32 and 64 bits read, written and appended most significant byte first, Go's `binary.BigEndian` |
| [csv](csv.md) | `csv.h` | CSV of RFC 4180 as Go reads and writes it: the reader, its rows, the writer, a text or a file of a program's records in one call |
| [csv::options](csv-options.md) | `csv.h` | the settings of a reader and a writer: the separator, comments, lazy quotes, leading spaces, the same count of fields, the bound of a record |
| [csv::reader](csv-reader.md) | `csv.h` | the records of a text or a stream one at a time, the header, a program's types by their fields |
| [csv::row](csv-row.md) | `csv.h` | one record: its fields by index and by the header's name, the line and the place of each field |
| [csv::writer](csv-writer.md) | `csv.h` | records into a stream, quoted where they need it, `\n` or `\r\n` |
| [error](error.md) | `error.h` | why an input is not what its format says, the same type for every format: the code, the offset, the line and the column, the path, the error of a stream, `message()` |
| [field](field.md) | `fields.h` | one field of a description: `required`, `omit_empty`, `quoted`, `names`, `tagged`, `attribute`, `text` |
| [field_list](field_list.md) | `fields.h` | the description of a program's type by its fields, `describe(field_list&)`, read and written by JSON, CSV and XML alike |
| [hex](hex.md) | `hex.h` | hexadecimal, two characters a byte: `encode`, `encode_upper`, `decode` of either case, `dump` as `hexdump -C`, the streams |
| [hex::decoder](hex-decoder.md) | `hex.h` | a reader of the bytes another reader's hexadecimal decodes to |
| [hex::dumper](hex-dumper.md) | `hex.h` | a writer that writes the dump of what it is given to another writer, Go's `hex.Dumper` |
| [hex::encoder](hex-encoder.md) | `hex.h` | a writer that writes the hexadecimal of what it is given to another writer |
| [json](json.md) | `json.h` | one JSON value, immutable: `parse`, `to_string`, the lookups, `set`, `erase`, `push_back`, JSON Pointer, `==` and `hash`; a program's types (`parse<T>`, `stringify`, `as<T>`), files (`load`, `save`) |
| [json::builder](json-builder.md) | `json.h` | an array or an object made a member at a time, without a copy per step |
| [json::member](json-member.md) | `json.h` | a key and its value, what an object holds |
| [json::options](json-options.md) | `json.h` | what a reading accepts: the depth, duplicate keys, invalid UTF-8, numbers kept as text, unknown fields, the size of a token |
| [json::reader](json-reader.md) | `json.h` | JSON a token or a value at a time, from a text or a stream: `next`, `more`, `read`, `skip` |
| [json::style](json-style.md) | `json.h` | how a value is written: compact, or indented (`json::compact`, `json::pretty`) |
| [json::token](json-token.md) | `json.h` | a token of the reader: its kind and its text |
| [json::writer](json-writer.md) | `json.h` | JSON into a stream, a token or a value at a time, its structure checked |
| [little_endian](little_endian.md) | `binary.h` | numbers of 16, 32 and 64 bits read, written and appended least significant byte first, Go's `binary.LittleEndian` |
| [pem](pem.md) | `pem.h` | a block of RFC 7468, its type, headers and bytes: `parse`, `parse_all`, `to_string` |
| [varint](varint.md) | `binary.h` | the variable-length integers of Go and protobuf, unsigned and zigzag-signed, from bytes and from a buffered reader |
| [xml](xml.md) | `xml.h` | a node of an XML tree that never changes: `parse`, the names, attributes, children and text, `set`, `erase`, `push_back`, `to_string`; a program's types; XML 1.0 fifth edition with namespaces, no DTD, UTF-16 and 27 single-byte encodings |
| [xml::attr](xml-attr.md) | `xml.h` | an attribute as the document writes it: its name, its value, its namespace |
| [xml::builder](xml-builder.md) | `xml.h` | an element made an attribute and a child at a time, without a copy per step |
| [xml::options](xml-options.md) | `xml.h` | what a reading accepts and keeps: the depth, the size of a token, comments and white space |
| [xml::reader](xml-reader.md) | `xml.h` | XML a token at a time, from a text or a stream: `next`, `peek`, `read`, `skip` |
| [xml::style](xml-style.md) | `xml.h` | how a node is written: on one line or indented, with or without the declaration (`xml::compact`, `xml::pretty`) |
| [xml::token](xml-token.md) | `xml.h` | a token of the reader: its kind, its names, attributes and text |
| [xml::writer](xml-writer.md) | `xml.h` | XML into a stream: `start`, `attribute`, `text`, `cdata`, `comment`, `instruction`, `end`, `node`, `flush` |

## Enumerations

| Enumeration | Header | Description |
|---|---|---|
| [errc](errc.md) | `error.h` | the codes of every format's errors, in the category `encoding` |
| [json::kind](json-kind.md) | `json.h` | the kind of a JSON value: null, boolean, number, string, array, object |
| [json::token::kind](json-token-kind.md) | `json.h` | the kind of a token of the JSON reader, a key a kind of its own |
| [xml::kind](xml-kind.md) | `xml.h` | the kind of a node of a tree: an element, a text, a comment, an instruction, or none |
| [xml::token::kind](xml-token-kind.md) | `xml.h` | the kind of a token of the XML reader: a start, an end, a text, a comment, an instruction, the DOCTYPE |

## See also

- [Benchmarks](benchmarks.md): the byte codecs, JSON, CSV and XML against Go's packages
- [expected](../core/expected.md): the value or the error
- [io](../io/README.md): the streams a codec reads and writes
- [hash](../hash/README.md): checksums of the same bytes
- [The modules](../README.md)
