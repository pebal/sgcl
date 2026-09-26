# sgcl::encoding

What Go has in `encoding/...`: the formats data is written in when it leaves a program and read in when it comes back. `#include "sgcl/encoding/encoding.h"` brings the module in; it depends on [`core`](../core/README.md), [`txt`](../txt/README.md) (the character encodings an XML declaration names), [`async`](../async/README.md) and [`io`](../io/README.md) (a codec is a stream as well as a function). The index of the whole interface is [`docs/sgcl/`](../README.md).

Today the module holds the byte codecs — base64, base32, hex, ascii85, PEM and the binary numbers — JSON (the value, a reader of tokens and values, a writer), CSV, XML (a reader of tokens, a writer and a tree), and the description of a program's own types by their fields, which JSON, CSV and XML read and write.

## One namespace, a type for each format

The module is the namespace `sgcl::encoding`, as every module but core is a namespace of its own. Inside it, every format is one type named as the format is named — `base64`, `base32`, `hex`, `ascii85`, `pem`, `big_endian`, `little_endian`, `varint`, `json`, `csv`, `xml` — and everything that belongs to a format is a member of it: `encoding::base64::standard`, `encoding::base64::encoder`, `encoding::pem::parse`. `encoding::base64::standard.encode(b)` reads as Go's `base64.StdEncoding.EncodeToString(b)`, and after `using namespace sgcl::encoding;` a program writes `base64::standard.encode(b)`. The errors of the module are `encoding::error` and `encoding::errc`, and each format names the same type `error` (`base64::error`, `pem::error`).

A codec that has something to choose — an alphabet, the padding, strict or lenient — is a **value** with `encode` and `decode` as its methods, and the choices Go names are constants of it: `encoding::base64::url`, `encoding::base32::hex`. A codec with nothing to choose (`hex`, `ascii85`, `varint`) has the same methods as static ones. So every codec is used the same way, and a codec of one's own — base64 with the alphabet of `crypt(3)` — is one line.

## Strict by default

A decoding refuses what it cannot tell apart. base64 and base32 refuse a character outside the alphabet, a line ending included (RFC 4648 section 3.3), and bits past the data in the last character (section 3.5), which would let two different texts decode to the same bytes — what a signature, a key or a token must not allow. `lenient()` takes both, for MIME and the encoders that wrap their lines; it is Go's default, so a text Go reads, `lenient()` reads. PEM decodes its base64 strictly too. An ascii85 group worth more than 32 bits is refused where Go takes it modulo 2^32.

## Errors are values, with a place

Nothing in the module throws on its input. A function that decodes a whole text returns `expected<T, error>`; a decoder read as a stream fails its read with an `io::error` and keeps the `error` in `last_error()`. [`error`](error.md) is one type for every format, under each format's name (`encoding::base64::error`, `encoding::pem::error`, `encoding::json::error`): the code, the byte of the input it was found at, the line and the column when the format has lines, the path inside the structure when it has one (JSON, XML), and the error of the stream when a stream failed. The offset is where the input stops being the start of something valid, so `QQ=x` fails at the `x` and a cut text at its end; `message()` reads `offset 17: invalid character '*'` or `3:10: END B does not match BEGIN A`. The line and the column are counted from the text after the fact, when something failed: a decoding that succeeds never counts a line ending.

What throws is a mistake in the program, not in its input: an alphabet with a repeated character (`invalid_argument`, an error at compile time in a constant), a caller's buffer smaller than the size the codec asked for (`length_error`), a PEM type that is not a label.

## Memory

A codec is plain data — the characters of its alphabet and a table back — and holds no tracked pointer, so the constants are constants and a codec may live anywhere. What a codec returns is the library's: a [`string`](../core/string.md) for text and a [`vector<byte>`](../core/vector.md) for bytes; the input is a `slice<const byte>` (a vector, a string's bytes, a raw buffer) or a `const string&`. `encode_to` and `decode_to` write into the caller's buffer and allocate nothing. A stream codec is a managed object holding its block, 8 KB of `array<byte, N>` as io's buffers are, and the stream under it.

## Pages

| page | header | what it is |
|---|---|---|
| [error](error.md) | `sgcl/encoding/error.h` | `errc` (the codes of every format), `error` (`code`, `offset`, `line`, `column`, `path`, `io_error`, `message`) |
| [base64](base64.md) | `sgcl/encoding/base64.h` | `base64`: `standard`, `url`, `raw_standard`, `raw_url`, an alphabet of one's own, `without_padding`, `lenient`, `encode`, `decode`, `encoded_size`, `max_decoded_size`, `encode_to`, `decode_to`, `encoder_to`, `decoder_from` |
| [base32](base32.md) | `sgcl/encoding/base32.h` | `base32`: `standard`, `hex`, the members of base64 |
| [hex](hex.md) | `sgcl/encoding/hex.h` | `hex`: `encode`, `encode_upper`, `decode`, `dump`, `dumper_to` |
| [ascii85](ascii85.md) | `sgcl/encoding/ascii85.h` | `ascii85`: `encode`, `decode`, `encoder_to`, `decoder_from` |
| [pem](pem.md) | `sgcl/encoding/pem.h` | `pem`: a block of RFC 7468 (`type`, `bytes`, `headers`), `parse`, `parse_all`, `to_string` |
| [binary](binary.md) | `sgcl/encoding/binary.h` | `big_endian`, `little_endian` (`read_u16`…`append_u64`), `varint` (`append`, `write`, `read`, the signed forms, from a buffered reader) |
| [json](json.md) | `sgcl/encoding/json.h` | `json`: one value, immutable (`parse`, `to_string`, `[]`, `as_int`…, `members`, `set`, `erase`, `push_back`, `at_path`, `set_path`, `==`, `hash`), `json::builder`, `options`, `style` |
| [json::reader](json_reader.md) | `sgcl/encoding/json.h` | `json::reader` (`next`, `more`, `read`, `skip`, `last_error`), `json::token`: JSON a piece at a time, from a text or a stream |
| [json::writer](json_writer.md) | `sgcl/encoding/json.h` | `json::writer` (`begin_object`, `key`, `value`, `end_object`, `flush`): JSON into a stream |
| [csv](csv.md) | `sgcl/encoding/csv.h` | `csv::reader` (`read_header`, `next`, `rows`, `read<T>`), `csv::row` (fields by index and by the header's name, `position`), `csv::writer` (`write`, `use_crlf`, `flush`), `options` |
| [field_list](fields.md) | `sgcl/encoding/fields.h` | `field_list`, `field`: a type of the program described by `describe(field_list&)` (`add`, `required`, `omit_empty`, `as_string`, `names`, `tagged`, `attribute`, `text`), read and written by every format; `json::parse<T>`, `stringify`, `as<T>` |
| [xml](xml.md) | `sgcl/encoding/xml.h` | `xml`: a node of a tree that never changes (`parse`, `name`, `attribute`, `children`, `child`, `text`, `set`, `erase`, `push_back`, `to_string`), `xml::builder`, a program's types (`parse<T>`, `as<T>`, `from`, `stringify`); XML 1.0 fifth edition with namespaces, no DTD, UTF-16 and 27 single byte encodings |
| [xml::reader](xml-reader.md) | `sgcl/encoding/xml.h` | `xml::reader` (`next`, `peek`, `read`, `skip` and their `async_` forms, `last_error`), `xml::token` — from a string or a stream a piece at a time |
| [xml::writer](xml-writer.md) | `sgcl/encoding/xml.h` | `xml::writer`: `start`, `attribute`, `text`, `cdata`, `comment`, `instruction`, `end`, `node`, `flush`, `async_flush` |

## SGCL and Go

| Go | SGCL | note |
|---|---|---|
| `base64.StdEncoding`, `URLEncoding`, `RawStdEncoding`, `RawURLEncoding` | `encoding::base64::standard`, `url`, `raw_standard`, `raw_url` | |
| `base64.NewEncoding(alphabet)`, `WithPadding`, `Strict` | `encoding::base64(alphabet, padding)`, `without_padding()`, strict by default and `lenient()` | Go's default is `lenient()` |
| `EncodeToString`, `DecodeString` | `encode`, `decode` | `encode` takes bytes or the bytes of a text |
| `Encode`, `Decode`, `EncodedLen`, `DecodedLen` | `encode_to`, `decode_to`, `encoded_size`, `max_decoded_size` | a buffer too small is `length_error`; a size past `size_t` is `SIZE_MAX` |
| `NewEncoder`, `NewDecoder` | `encoder_to(w)`, `decoder_from(r)` | an `io::writer` and an `io::closer`, an `io::reader`; `close()` leaves the writer under it open, as Go's does; `co_await write`, `read`, `close` |
| `CorruptInputError` | `encoding::base64::error` (`error`) | the code, the offset and a message |
| `base32.StdEncoding`, `HexEncoding` | `encoding::base32::standard`, `encoding::base32::hex` | the members of base64 |
| `hex.EncodeToString`, `DecodeString` | `encoding::hex::encode`, `encoding::hex::decode` | `encode_upper`; the decoding takes either case |
| `hex.Dump`, `hex.Dumper` | `encoding::hex::dump`, `encoding::hex::dumper_to(w)` | the same lines |
| `ascii85.Encode`, `Decode`, `NewEncoder`, `NewDecoder` | `encoding::ascii85::encode`, `decode`, `encoder_to`, `decoder_from` | a group past 32 bits refused |
| `pem.Block`, `Decode`, `Encode`, `EncodeToMemory` | `pem`, `encoding::pem::parse`, `parse_all`, `to_string()` | a malformed block is an error with a line and a column, not skipped; the headers in their order |
| `binary.BigEndian.Uint32`, `PutUint32`, `AppendUint32` (and 16, 64, `LittleEndian`) | `encoding::big_endian::read_u32`, `write_u32`, `append_u32` (and 16, 64, `little_endian`) | |
| `binary.AppendUvarint`, `PutUvarint`, `Uvarint`, `ReadUvarint` | `encoding::varint::append`, `write`, `read(bytes)`, `read(buffered_reader&)` | the signed forms with `_signed`; the end of a stream before a number is `nullopt` |
| `binary.Read`, `binary.Write` of structures | — | with the mapping of types, after JSON |
| `json.Unmarshal` into `any`, `json.Marshal` | `encoding::json::parse`, `to_string` | immutable; integers exact; the defaults are v2's |
| `json.Decoder`, `Token`, `More`, v2 `jsontext` | `encoding::json::reader`: `next`, `more`, `read`, `skip` | a key is a token of its own kind |
| `json.Encoder`, v2 `jsontext.Encoder` | `encoding::json::writer` | the structure checked; the mistake reported by `flush()` |
| struct tags, `Marshal`/`Unmarshal` of a struct | `describe(field_list&)`, `json::stringify`, `json::parse<T>` | one description for every format |
| `csv.Reader`, `csv.Writer` | `encoding::csv::reader`, `encoding::csv::writer` | the header, positions in code points, records as types |
| `xml.NewDecoder`, `Token`, `Skip`, `DecodeElement` | `encoding::xml::reader`, `next`, `skip`, `peek` and `read` | strict always; no DTD; UTF-16 and the single byte encodings built in |
| `xml.NewEncoder`, `EncodeToken`, `Indent`, `Flush` | `encoding::xml::writer`, `start`…`end`, `style{indent}`, `flush` | |
| `xml.Unmarshal`, `xml.Marshal` of a struct | `encoding::xml::parse<T>`, `stringify`; `xml::parse` and `to_string` of a tree | the same `describe(field_list&)` as JSON's; `attribute()`, `text()`; Go has no tree |
| `encoding/asn1` | — | with `crypto` |
| `encoding/gob` | — | a serialization of object graphs, after JSON |

Where the two differ in what they accept, it is written on the codec's page and held to in the tests by name: a line ending in a strict decoding, bits past the data, what follows base32's padding, the byte `0xFF` that Go's base32 without padding takes for padding, an ascii85 group past 32 bits, a tenth varint byte with its high bit set, bits past the data in a PEM block, a malformed PEM block.

## Tests

JSON (`tests/encoding/json_*.cpp`) against Go's v2 (`tools/json_oracle.go`): a million doubles and a million floats written, half a million literals read both ways, every file of JSONTestSuite decided as v2 decides it with its tokens and its value written back; the reader fed 1, 2, 3 and 7 bytes at a time; differential fuzzing against Go (`tools/json_fuzz.go`, run with `SGCL_JSON_FUZZ`). Typed JSON against Go's `Marshal` of the same structure. CSV (`tests/encoding/csv.cpp`) against Go's `encoding/csv` (`tools/csv_oracle.go`): named texts with their fields' places and errors, a hundred thousand random texts, whole and in pieces, and the text Go's writer writes.

The byte codecs: `tests/encoding` against Go's packages as the oracle (`tools/encoding_oracle.go` writes `tests/encoding/encoding_tests.h`): the vectors of RFC 4648 section 10; every length of input from 0 to 300 through the ten codecs; every byte from 0 to 255 at every position of a valid text of each codec, accepted or refused and where; varints both ways and every way a read can fail; blocks Go writes and texts Go reads for PEM, and the fourteen examples of RFC 7468 itself. The streams are fed and read in pieces of 1, 2, 3 and 7 bytes, and must come out as the whole text does; the sizes are held at the edge of `size_t`.

XML against the W3C XML Conformance Test Suite (read from `~/Programming/oracles/xmlconf` when it is there; every test decided otherwise by design is named with its reason) and against Go's `encoding/xml` (`tools/xml_oracle.go` writes `tests/encoding/xml_go_tests.h`): the tokens of the suite's documents and of named ones, and, with `-fuzz`, of mutated ones; a stream in pieces of every small size gives what the whole document gives, errors and their places included; the attacks (billion laughs, XXE, the quadratic blowup), the limits, and a token fed a byte at a time read in linear time.
