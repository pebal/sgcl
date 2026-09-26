// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle for the byte codecs of sgcl/encoding (base64.h, base32.h,
// hex.h, ascii85.h, pem.h, binary.h): Go's encoding packages asked the
// same questions, the answers written out as a C++ header the tests
// include. Run it from the root of the tree:
//
//     go run tools/encoding_oracle.go > tests/encoding/encoding_tests.h
//
// What is asked, by name, because an oracle checks only what it is asked:
//
//   - every length of input from 0 to 300 bytes through the four base64
//     alphabets, the two base32 ones with and without padding, hex and
//     ascii85; the inputs come from a generator both sides write the same
//     (splitmix64), and the header keeps an FNV-1a hash of each text
//     rather than the text, 3010 of them;
//   - every byte from 0 to 255 at every position of a few valid texts of
//     each codec, and one byte more at the end: accepted or refused, and
//     where Go says the input went wrong. base64 twice, against Go's
//     Strict() and against its default; base32 once, Go having no strict
//     decoding of it; hex with the kind of error, its errors carrying no
//     offset;
//   - varints: numbers to bytes both ways, signed and not, the bytes of a
//     read cut short or too long, from a slice and from a stream;
//   - PEM: blocks written by EncodeToMemory, texts Decode reads, and the
//     examples of RFC 7468 itself when ~/Programming/oracles/rfc7468 holds
//     its text (curl -o ~/Programming/oracles/rfc7468/rfc7468.txt
//     https://www.rfc-editor.org/rfc/rfc7468.txt).
//
// Where the two sides differ, and the test knows each by name:
//
//   - a line ending in the strict decoding: Go's Strict() still skips
//     '\r' and '\n', the strict decoding here refuses them (RFC 4648
//     section 3.3); lenient() is Go's default;
//   - bits past the data in the last character of base32: Go has no
//     strict base32 and takes them, the strict decoding here does not;
//   - a short group of base32 without padding that cannot end there (1, 3
//     or 6 characters): Go returns nothing for it and no error;
//   - a character after the padding of base32: Go drops a group of fewer
//     than eight characters after a complete one, whatever they are
//     ("MY======M" is "f" there);
//   - the byte 0xFF in base32 without padding: Go takes it for padding
//     (its NoPadding is -1, and -1 as a byte is 0xFF);
//   - an ascii85 group worth more than 32 bits: Go takes it modulo 2^32;
//   - a tenth varint byte with its high bit set: past 64 bits here, cut
//     short for Go's Uvarint (its ReadUvarint says past 64 bits too);
//   - bits past the data in the last character of a PEM block's base64:
//     refused here (a key has one encoding), taken by Go's pem.Decode;
//   - the offset of a group that is cut short: Go names the first
//     character of the group, this names the end of the input.
package main

import (
	"bytes"
	"encoding/ascii85"
	"encoding/base32"
	"encoding/base64"
	"encoding/binary"
	"encoding/hex"
	"encoding/pem"
	"errors"
	"fmt"
	"io"
	"os"
	"strings"
)

// The inputs: splitmix64 seeded by the length, a byte of each output,
// the same generator as tests/encoding/codecs.cpp's
func input(n int) []byte {
	state := uint64(n)*0x9E3779B97F4A7C15 + 1
	out := make([]byte, n)
	for i := range out {
		state += 0x9E3779B97F4A7C15
		z := state
		z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9
		z = (z ^ (z >> 27)) * 0x94D049BB133111EB
		z ^= z >> 31
		out[i] = byte(z >> 17)
	}
	return out
}

func fnv(s []byte) uint64 {
	h := uint64(0xcbf29ce484222325)
	for _, b := range s {
		h ^= uint64(b)
		h *= 0x100000001b3
	}
	return h
}

func a85(b []byte) string {
	out := make([]byte, ascii85.MaxEncodedLen(len(b)))
	n := ascii85.Encode(out, b)
	return string(out[:n])
}

type codec struct {
	name string
	enc  func([]byte) string
}

var codecs = []codec{
	{"base64::standard", base64.StdEncoding.EncodeToString},
	{"base64::url", base64.URLEncoding.EncodeToString},
	{"base64::raw_standard", base64.RawStdEncoding.EncodeToString},
	{"base64::raw_url", base64.RawURLEncoding.EncodeToString},
	{"base32::standard", base32.StdEncoding.EncodeToString},
	{"base32::hex", base32.HexEncoding.EncodeToString},
	{"base32::standard.without_padding()", base32.StdEncoding.WithPadding(base32.NoPadding).EncodeToString},
	{"base32::hex.without_padding()", base32.HexEncoding.WithPadding(base32.NoPadding).EncodeToString},
	{"hex::encode", hex.EncodeToString},
	{"ascii85::encode", a85},
}

// A decoder of Go's, reduced to what the table keeps: -1 for accepted,
// the offset for refused
type decoder struct {
	name    string
	lenient bool
	decode  func(string) int
	encode  func([]byte) string
	sizes   []int
}

func offsetOf(err error) int {
	if err == nil {
		return -1
	}
	var b64 base64.CorruptInputError
	if errors.As(err, &b64) {
		return int(b64)
	}
	var b32 base32.CorruptInputError
	if errors.As(err, &b32) {
		return int(b32)
	}
	var a ascii85.CorruptInputError
	if errors.As(err, &a) {
		return int(a)
	}
	panic(err)
}

func b64(e *base64.Encoding) func(string) int {
	return func(s string) int {
		_, err := e.DecodeString(s)
		return offsetOf(err)
	}
}

func b32(e *base32.Encoding) func(string) int {
	return func(s string) int {
		_, err := e.DecodeString(s)
		return offsetOf(err)
	}
}

func a85dec(s string) int {
	dst := make([]byte, 4*len(s)+4)
	_, _, err := ascii85.Decode(dst, []byte(s), true)
	return offsetOf(err)
}

var decoders = []decoder{
	{"base64::standard", false, b64(base64.StdEncoding.Strict()), base64.StdEncoding.EncodeToString, []int{4, 5}},
	{"base64::standard", true, b64(base64.StdEncoding), base64.StdEncoding.EncodeToString, []int{4, 5}},
	{"base64::url", false, b64(base64.URLEncoding.Strict()), base64.URLEncoding.EncodeToString, []int{4, 5}},
	{"base64::url", true, b64(base64.URLEncoding), base64.URLEncoding.EncodeToString, []int{4, 5}},
	{"base64::raw_standard", false, b64(base64.RawStdEncoding.Strict()), base64.RawStdEncoding.EncodeToString, []int{4, 5}},
	{"base64::raw_standard", true, b64(base64.RawStdEncoding), base64.RawStdEncoding.EncodeToString, []int{4, 5}},
	{"base64::raw_url", false, b64(base64.RawURLEncoding.Strict()), base64.RawURLEncoding.EncodeToString, []int{4, 5}},
	{"base64::raw_url", true, b64(base64.RawURLEncoding), base64.RawURLEncoding.EncodeToString, []int{4, 5}},
	{"base32::standard", true, b32(base32.StdEncoding), base32.StdEncoding.EncodeToString, []int{3, 4}},
	{"base32::hex", true, b32(base32.HexEncoding), base32.HexEncoding.EncodeToString, []int{3, 4}},
	{"base32::standard.without_padding()", true, b32(base32.StdEncoding.WithPadding(base32.NoPadding)), base32.StdEncoding.WithPadding(base32.NoPadding).EncodeToString, []int{3, 4}},
	{"base32::hex.without_padding()", true, b32(base32.HexEncoding.WithPadding(base32.NoPadding)), base32.HexEncoding.WithPadding(base32.NoPadding).EncodeToString, []int{3, 4}},
	{"ascii85", false, a85dec, a85, []int{6, 7}},
}

const digits = "0123456789abcdefghijklmnopqrstuvwxyz"

func outcome(off int) byte {
	if off < 0 {
		return '.'
	}
	return digits[off]
}

func cString(s string) string {
	var b strings.Builder
	b.WriteByte('"')
	for i := 0; i < len(s); i++ {
		c := s[i]
		switch {
		case c == '"' || c == '\\':
			b.WriteByte('\\')
			b.WriteByte(c)
		case c == '\n':
			b.WriteString("\\n")
		case c == '\r':
			b.WriteString("\\r")
		case c < 0x20 || c >= 0x7F:
			fmt.Fprintf(&b, "\\x%02x\"\"", c)
		default:
			b.WriteByte(c)
		}
	}
	b.WriteByte('"')
	return b.String()
}

func main() {
	w := os.Stdout
	fmt.Fprint(w, `//------------------------------------------------------------------------------
// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//------------------------------------------------------------------------------
#pragma once

// Generated by tools/encoding_oracle.go: do not edit.
//
// What Go's encoding packages answer for the byte codecs; the source of
// that program lists what is asked and where the two sides differ.

#include <cstdint>

namespace oracle {
`)

	// 1. Every length through every codec
	fmt.Fprintln(w, "    // The FNV-1a hash of the text of input(n), n from 0 to 300, per codec")
	fmt.Fprintln(w, "    inline constexpr const char* EncodeCodecs[] = {")
	for _, c := range codecs {
		fmt.Fprintf(w, "        %q,\n", c.name)
	}
	fmt.Fprintln(w, "    };")
	fmt.Fprintf(w, "    inline constexpr uint64_t EncodeHashes[301][%d] = {\n", len(codecs))
	for n := 0; n <= 300; n++ {
		in := input(n)
		fmt.Fprint(w, "        {")
		for i, c := range codecs {
			if i > 0 {
				fmt.Fprint(w, ", ")
			}
			fmt.Fprintf(w, "0x%016xull", fnv([]byte(c.enc(in))))
		}
		fmt.Fprintln(w, "},")
	}
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w)

	// A few whole texts, so that a failure shows something to read
	fmt.Fprintln(w, "    struct EncodeText {")
	fmt.Fprintln(w, "        const char* codec;")
	fmt.Fprintln(w, "        int length;")
	fmt.Fprintln(w, "        const char* text;")
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w, "    inline constexpr EncodeText EncodeTexts[] = {")
	for _, c := range codecs {
		for _, n := range []int{0, 1, 2, 3, 4, 5, 6, 7, 8, 13} {
			fmt.Fprintf(w, "        {%q, %d, %s},\n", c.name, n, cString(c.enc(input(n))))
		}
	}
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w)

	// ascii85's 'z': zero groups on and off the boundary of a group
	fmt.Fprintln(w, "    struct Ascii85Case {")
	fmt.Fprintln(w, "        const char* hex;")
	fmt.Fprintln(w, "        const char* text;")
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w, "    inline constexpr Ascii85Case Ascii85Zeros[] = {")
	for _, h := range []string{"00", "0000", "000000", "00000000", "0000000000", "000000000000000000", "0100000000", "000000000100000000", "00000000ffffffff", "ffffffff", "ffffff", "ffff", "ff", "0000000000000000000000000000000000"} {
		b, _ := hex.DecodeString(h)
		fmt.Fprintf(w, "        {%q, %s},\n", h, cString(a85(b)))
	}
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w)

	// 2. Every byte at every position
	fmt.Fprintln(w, "    // A valid text with the byte at position replaced by each of the 256")
	fmt.Fprintln(w, "    // (at the text's length: one byte added at the end): '.' accepted,")
	fmt.Fprintln(w, "    // else the offset Go reports, a digit of base 36")
	fmt.Fprintln(w, "    struct ByteCase {")
	fmt.Fprintln(w, "        const char* codec;")
	fmt.Fprintln(w, "        bool lenient;")
	fmt.Fprintln(w, "        const char* text;")
	fmt.Fprintln(w, "        int position;")
	fmt.Fprintln(w, "        const char* outcomes;")
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w, "    inline constexpr ByteCase ByteCases[] = {")
	for _, d := range decoders {
		bases := []string{}
		for _, size := range d.sizes {
			bases = append(bases, d.encode(input(size + 100)[:size]))
		}
		if d.name == "ascii85" {
			bases = append(bases, "z87cU")
		}
		for _, base := range bases {
			for pos := 0; pos <= len(base); pos++ {
				var o [256]byte
				for b := 0; b < 256; b++ {
					var s string
					if pos == len(base) {
						s = base + string([]byte{byte(b)})
					} else {
						t := []byte(base)
						t[pos] = byte(b)
						s = string(t)
					}
					o[b] = outcome(d.decode(s))
				}
				fmt.Fprintf(w, "        {%q, %v, %q, %d, %q},\n", d.name, d.lenient, base, pos, string(o[:]))
			}
		}
	}
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w)

	// hex, whose errors carry the byte or the length but no offset
	fmt.Fprintln(w, "    // hex: '.' accepted, 'I' an invalid byte, 'L' an odd length")
	fmt.Fprintln(w, "    struct HexCase {")
	fmt.Fprintln(w, "        const char* text;")
	fmt.Fprintln(w, "        int position;")
	fmt.Fprintln(w, "        const char* outcomes;")
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w, "    inline constexpr HexCase HexCases[] = {")
	for _, base := range []string{"0f1E2d3C", "a9"} {
		for pos := 0; pos <= len(base); pos++ {
			var o [256]byte
			for b := 0; b < 256; b++ {
				var s string
				if pos == len(base) {
					s = base + string([]byte{byte(b)})
				} else {
					t := []byte(base)
					t[pos] = byte(b)
					s = string(t)
				}
				_, err := hex.DecodeString(s)
				var ib hex.InvalidByteError
				switch {
				case err == nil:
					o[b] = '.'
				case errors.As(err, &ib):
					o[b] = 'I'
				case errors.Is(err, hex.ErrLength):
					o[b] = 'L'
				default:
					panic(err)
				}
			}
			fmt.Fprintf(w, "        {%q, %d, %q},\n", base, pos, string(o[:]))
		}
	}
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w)

	// Hand-picked texts, with Go's offsets, for the names in the test
	fmt.Fprintln(w, "    struct DecodeCase {")
	fmt.Fprintln(w, "        const char* codec;")
	fmt.Fprintln(w, "        bool lenient;")
	fmt.Fprintln(w, "        const char* text;")
	fmt.Fprintln(w, "        int offset;   // -1: accepted")
	fmt.Fprintln(w, "        const char* hex;   // the bytes when accepted")
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w, "    inline constexpr DecodeCase DecodeCases[] = {")
	texts := []string{"", "Q", "QQ", "QQ=", "QQ==", "QQ=x", "QQ==x", "QQ===", "QR==", "QUI=", "QUJ=", "QUJD", "QUJDR", "QUJDRA", "QUJDRA=", "QUJDRA==", "Q===", "=", "==", "QUJ\nD", "QUJD\n", "\r\nQUJD", "Q=Q=", "QQ=\n=", "QQ\n==", "QUJDRA==QUJD", "QUJD=", "QUJDQ=", "QUJD\x01", "-_-_", "+/+/"}
	for _, d := range decoders[:8] {
		for _, t := range texts {
			e := d.decode(t)
			h := ""
			if e < 0 {
				var b []byte
				switch d.name {
				case "base64::standard":
					if d.lenient {
						b, _ = base64.StdEncoding.DecodeString(t)
					} else {
						b, _ = base64.StdEncoding.Strict().DecodeString(t)
					}
				case "base64::url":
					b, _ = base64.URLEncoding.DecodeString(t)
				case "base64::raw_standard":
					b, _ = base64.RawStdEncoding.DecodeString(t)
				case "base64::raw_url":
					b, _ = base64.RawURLEncoding.DecodeString(t)
				}
				h = hex.EncodeToString(b)
			}
			fmt.Fprintf(w, "        {%q, %v, %s, %d, %q},\n", d.name, d.lenient, cString(t), e, h)
		}
	}
	b32texts := []string{"", "M", "MY", "MY=", "MY======", "MZ======", "MY=====", "MY=======", "MFRA====", "MFRGG===", "MFRGGZA=", "MFRGGZDF", "MFR=====", "MFRGGZ==", "MFRGGZD=", "MFRGGZD", "MFRGGZ", "MFR", "MZXW6YTBOI======", "MZXW6YTBOI", "MZXW\n6YTB"}
	for _, d := range decoders[8:12] {
		for _, t := range b32texts {
			e := d.decode(t)
			h := ""
			if e < 0 {
				var b []byte
				switch d.name {
				case "base32::standard":
					b, _ = base32.StdEncoding.DecodeString(t)
				case "base32::hex":
					b, _ = base32.HexEncoding.DecodeString(t)
				case "base32::standard.without_padding()":
					b, _ = base32.StdEncoding.WithPadding(base32.NoPadding).DecodeString(t)
				case "base32::hex.without_padding()":
					b, _ = base32.HexEncoding.WithPadding(base32.NoPadding).DecodeString(t)
				}
				h = hex.EncodeToString(b)
			}
			fmt.Fprintf(w, "        {%q, %v, %s, %d, %q},\n", d.name, d.lenient, cString(t), e, h)
		}
	}
	a85texts := []string{"", "!", "!!", "z", "zz", "87cURD]i", "87cURD]", "87cUR", "87cU", "87c", "87", "8", "s8W-!", "s8W-\"", "s8W-", "s9", "~>", "<~87cURD]i~>", "87 cU\nRD]i", "abcdez", "8z", "87cUz", "zz!", "87cURD]iz!!", "!!!!!", "87\x01cURD]i", "87\x7fcURD]i", "uuuuu"}
	for _, t := range a85texts {
		dst := make([]byte, 4*len(t)+4)
		n, _, err := ascii85.Decode(dst, []byte(t), true)
		e := offsetOf(err)
		h := ""
		if e < 0 {
			h = hex.EncodeToString(dst[:n])
		}
		fmt.Fprintf(w, "        {\"ascii85\", false, %s, %d, %q},\n", cString(t), e, h)
	}
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w)

	// 3. varints
	fmt.Fprintln(w, "    struct VarintCase {")
	fmt.Fprintln(w, "        uint64_t value;")
	fmt.Fprintln(w, "        const char* hex;")
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w, "    struct SignedVarintCase {")
	fmt.Fprintln(w, "        int64_t value;")
	fmt.Fprintln(w, "        const char* hex;")
	fmt.Fprintln(w, "    };")
	unsigned := []uint64{0, 1, 2, 127, 128, 129, 255, 256, 300, 16383, 16384, 1<<21 - 1, 1 << 21, 1<<32 - 1, 1 << 32, 1 << 56, 1<<63 - 1, 1 << 63, 1<<64 - 1}
	state := uint64(12345)
	for i := 0; i < 64; i++ {
		state = state*6364136223846793005 + 1442695040888963407
		unsigned = append(unsigned, state>>(uint(i)%64))
	}
	fmt.Fprintln(w, "    inline constexpr VarintCase Uvarints[] = {")
	for _, v := range unsigned {
		fmt.Fprintf(w, "        {%dull, %q},\n", v, hex.EncodeToString(binary.AppendUvarint(nil, v)))
	}
	fmt.Fprintln(w, "    };")
	signed := []int64{0, 1, -1, 2, -2, 63, -64, 64, -65, 127, -128, 1<<31 - 1, -1 << 31, 1<<63 - 1, -1 << 63}
	for i := 0; i < 64; i++ {
		state = state*6364136223846793005 + 1442695040888963407
		signed = append(signed, int64(state)>>(uint(i)%64))
	}
	fmt.Fprintln(w, "    inline constexpr SignedVarintCase Varints[] = {")
	for _, v := range signed {
		if v == -1<<63 {
			fmt.Fprintf(w, "        {INT64_MIN, %q},\n", hex.EncodeToString(binary.AppendVarint(nil, v)))
		} else {
			fmt.Fprintf(w, "        {%dll, %q},\n", v, hex.EncodeToString(binary.AppendVarint(nil, v)))
		}
	}
	fmt.Fprintln(w, "    };")

	fmt.Fprintln(w, "    // Bytes read as a uvarint: the value and the bytes taken; n 0 for")
	fmt.Fprintln(w, "    // bytes that end first, -k for a number past 64 bits at byte k - 1;")
	fmt.Fprintln(w, "    // stream: what ReadUvarint says of the same bytes as a stream,")
	fmt.Fprintln(w, "    // 'v' a value, 'E' the end before the first byte, 'U' cut short,")
	fmt.Fprintln(w, "    // 'O' past 64 bits")
	fmt.Fprintln(w, "    struct VarintRead {")
	fmt.Fprintln(w, "        const char* hex;")
	fmt.Fprintln(w, "        uint64_t value;")
	fmt.Fprintln(w, "        int n;")
	fmt.Fprintln(w, "        char stream;")
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w, "    inline constexpr VarintRead VarintReads[] = {")
	reads := []string{"", "00", "01", "7f", "8001", "8000", "80", "8080", "808080808080808080", "ffffffffffffffffff01", "ffffffffffffffffff02", "ffffffffffffffffff7f", "ffffffffffffffffff80", "ffffffffffffffffff8001", "8080808080808080808000", "80808080808080808001", "8080808080808080800100", "ac02ff", "ffffffffffffffff7f", "ffffffffffffffffff00"}
	for _, h := range reads {
		b, _ := hex.DecodeString(h)
		v, n := binary.Uvarint(b)
		sv, err := binary.ReadUvarint(bytes.NewReader(b))
		s := byte('v')
		switch {
		case err == io.EOF:
			s = 'E'
		case err == io.ErrUnexpectedEOF:
			s = 'U'
		case err != nil:
			s = 'O'
		default:
			if sv != v {
				panic("the two reads differ")
			}
		}
		fmt.Fprintf(w, "        {%q, %dull, %d, '%c'},\n", h, v, n, s)
	}
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w)

	// 4. PEM
	fmt.Fprintln(w, "    // A block written by pem.EncodeToMemory: the type, the headers in the")
	fmt.Fprintln(w, "    // order Go writes them (\"Name: value\" lines), input(bytes), the text")
	fmt.Fprintln(w, "    struct PemEncode {")
	fmt.Fprintln(w, "        const char* type;")
	fmt.Fprintln(w, "        const char* headers;")
	fmt.Fprintln(w, "        int bytes;")
	fmt.Fprintln(w, "        const char* text;")
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w, "    inline constexpr PemEncode PemEncodes[] = {")
	type blk struct {
		t string
		h map[string]string
		n int
	}
	blocks := []blk{
		{"CERTIFICATE", nil, 0},
		{"CERTIFICATE", nil, 1},
		{"CERTIFICATE", nil, 47},
		{"CERTIFICATE", nil, 48},
		{"CERTIFICATE", nil, 49},
		{"EC PRIVATE KEY", nil, 121},
		{"X509 CRL", nil, 300},
		{"RSA PRIVATE KEY", map[string]string{"Proc-Type": "4,ENCRYPTED", "DEK-Info": "DES-EDE3-CBC,0123456789ABCDEF"}, 64},
		{"TEST", map[string]string{"B": "2", "A": "1", "Proc-Type": "4,ENCRYPTED"}, 5},
		{"TEST", map[string]string{"Comment": "a value, with: a colon"}, 3},
		{"", nil, 2},
	}
	for _, b := range blocks {
		text := string(pem.EncodeToMemory(&pem.Block{Type: b.t, Headers: b.h, Bytes: input(b.n)}))
		// the headers in Go's own order, read back from what it wrote
		hs := ""
		if len(b.h) > 0 {
			lines := strings.Split(text, "\n")
			for _, l := range lines[1:] {
				if l == "" {
					break
				}
				hs += l + "\n"
			}
		}
		fmt.Fprintf(w, "        {%q, %s, %d, %s},\n", b.t, cString(hs), b.n, cString(text))
	}
	fmt.Fprintln(w, "    };")

	fmt.Fprintln(w, "    // A text pem.Decode reads: the type, the headers sorted by name")
	fmt.Fprintln(w, "    // (\"Name: value\" lines), the bytes, and the offset of the rest")
	fmt.Fprintln(w, "    struct PemDecode {")
	fmt.Fprintln(w, "        const char* text;")
	fmt.Fprintln(w, "        const char* type;")
	fmt.Fprintln(w, "        const char* headers;")
	fmt.Fprintln(w, "        const char* hex;")
	fmt.Fprintln(w, "        int rest;")
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w, "    inline constexpr PemDecode PemDecodes[] = {")
	pemTexts := []string{
		"-----BEGIN A-----\nQUJD\n-----END A-----\n",
		"-----BEGIN A-----\r\nQUJD\r\nRA==\r\n-----END A-----\r\n",
		"explanatory text\n-----BEGIN X509 CRL-----\nQUJD RA==\n-----END X509 CRL-----\ntrailing\n",
		"-----BEGIN A-----\nQU\tJD\n  RA==  \n-----END A-----",
		"-----BEGIN A-----\n-----END A-----\n",
		"-----BEGIN RSA PRIVATE KEY-----\nProc-Type: 4,ENCRYPTED\nDEK-Info: DES-EDE3-CBC,0011\n\nQUJD\n-----END RSA PRIVATE KEY-----\n",
		"-----BEGIN A-----\nX: 1\nQUJD\n-----END A-----\n",
		"-----BEGIN A-----  \nQUJD\n-----END A-----\t\n-----BEGIN B-----\nRA==\n-----END B-----\n",
		"-----BEGIN A-----\nQUJD\n-----END A-----\n\n\n-----BEGIN B-----\nRA==\n-----END B-----\n",
		// an indented line of base64 after headers with no empty line:
		// the base64, not a folded value
		"-----BEGIN A-----\nA: 1\n QUJD\n-----END A-----\n",
		"-----BEGIN A-----\nA: 1\nB: 2\n\tQUJD\n RA==\n-----END A-----\n",
	}
	for _, t := range pemTexts {
		b, rest := pem.Decode([]byte(t))
		if b == nil {
			panic("pem: " + t)
		}
		hs := ""
		keys := []string{}
		for k := range b.Headers {
			keys = append(keys, k)
		}
		sortStrings(keys)
		for _, k := range keys {
			hs += k + ": " + b.Headers[k] + "\n"
		}
		fmt.Fprintf(w, "        {%s, %q, %s, %q, %d},\n", cString(t), b.Type, cString(hs), hex.EncodeToString(b.Bytes), len(t)-len(rest))
	}
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w)

	// The examples of RFC 7468 itself, read from the copy in the oracles
	// directory when it is there: every BEGIN to END of the text, with the
	// lines of explanatory text right above it (section 5.2), and what
	// pem.Decode makes of it
	fmt.Fprintln(w, "    // The examples of RFC 7468, sections 5 to 13 and appendix A, as the")
	fmt.Fprintln(w, "    // RFC prints them, and what pem.Decode reads from each")
	fmt.Fprintln(w, "    struct RfcPem {")
	fmt.Fprintln(w, "        const char* figure;")
	fmt.Fprintln(w, "        const char* text;")
	fmt.Fprintln(w, "        const char* type;")
	fmt.Fprintln(w, "        const char* hex;")
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w, "    inline constexpr RfcPem RfcPems[] = {")
	home, _ := os.UserHomeDir()
	if rfc, err := os.ReadFile(home + "/Programming/oracles/rfc7468/rfc7468.txt"); err == nil {
		lines := strings.Split(strings.ReplaceAll(string(rfc), "\r", ""), "\n")
		for i := 0; i < len(lines); i++ {
			if !strings.HasPrefix(lines[i], "-----BEGIN ") {
				continue
			}
			from := i
			for from > 0 && lines[from-1] != "" && !strings.HasPrefix(lines[from-1], " ") {
				from--
			}
			to := i
			for !strings.HasPrefix(lines[to], "-----END ") {
				to++
			}
			text := strings.Join(lines[from:to+1], "\n") + "\n"
			figure := ""
			for k := to + 1; k < len(lines) && k < to+4; k++ {
				if t := strings.TrimSpace(lines[k]); strings.HasPrefix(t, "Figure") {
					figure = t
				}
			}
			b, _ := pem.Decode([]byte(text))
			if b == nil {
				panic("rfc7468: " + figure)
			}
			fmt.Fprintf(w, "        {%q, %s, %q, %q},\n", figure, cString(text), b.Type, hex.EncodeToString(b.Bytes))
			i = to
		}
	}
	fmt.Fprintln(w, "    };")
	fmt.Fprintln(w)

	// 5. hex.Dump
	fmt.Fprintln(w, "    // The FNV-1a hash of hex.Dump(input(n)), n from 0 to 70")
	fmt.Fprintln(w, "    inline constexpr uint64_t DumpHashes[] = {")
	for n := 0; n <= 70; n++ {
		fmt.Fprintf(w, "        0x%016xull,\n", fnv([]byte(hex.Dump(input(n)))))
	}
	fmt.Fprintln(w, "    };")
	fmt.Fprintf(w, "    inline constexpr const char* Dump40 = %s;\n", cString(hex.Dump(input(40))))
	fmt.Fprintln(w, "}")
}

func sortStrings(s []string) {
	for i := 1; i < len(s); i++ {
		for j := i; j > 0 && s[j] < s[j-1]; j-- {
			s[j], s[j-1] = s[j-1], s[j]
		}
	}
}
