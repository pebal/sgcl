// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The differential fuzzing of sgcl/encoding/json.h against Go's v2: texts
// made by mutating the files of JSONTestSuite (those up to 4 KB) and a
// few of its own, each with what Go says of it, written to a file that
// the test JsonFuzz_Tests.AgainstGo reads when SGCL_JSON_FUZZ names it:
//
//     go run tools/json_fuzz.go <count> <seed> > <file>
//     SGCL_JSON_FUZZ=<file> tests_encoding --gtest_filter=JsonFuzz*
//
// A record: the length of the text (4 bytes, little-endian) and the text;
// a byte of flags (1: jsontext takes it as one value, 2: Unmarshal into
// any takes it); the FNV-1a hash of its tokens (8 bytes, as
// tools/json_oracle.go makes it); the length and the text of the value
// written back as json.h writes it (compact).
package main

import (
	"bytes"
	"encoding/binary"
	"encoding/json/jsontext"
	jsonv2 "encoding/json/v2"
	"io"
	"math"
	"math/rand"
	"os"
	"path/filepath"
	"strconv"
	"strings"
)

type fnv struct{ h uint64 }

func (f *fnv) add(b []byte) {
	for _, c := range b {
		f.h ^= uint64(c)
		f.h *= 0x100000001b3
	}
}

func canonicalNumber(lit string) string {
	if !strings.ContainsAny(lit, ".eE") {
		if lit == "-0" {
			return "-0"
		}
		if i, err := strconv.ParseInt(lit, 10, 64); err == nil {
			return strconv.FormatInt(i, 10)
		}
		if u, err := strconv.ParseUint(lit, 10, 64); err == nil {
			return strconv.FormatUint(u, 10)
		}
		return lit
	}
	f, err := strconv.ParseFloat(lit, 64)
	if err != nil {
		panic(err)
	}
	b, err := jsonv2.Marshal(f)
	if err != nil {
		panic(err)
	}
	return string(b)
}

func canonical(data []byte) (string, bool) {
	var v any
	if jsonv2.Unmarshal(data, &v) != nil {
		return "", false
	}
	dec := jsontext.NewDecoder(bytes.NewReader(data))
	type level struct {
		object, afterKey bool
		count            int
	}
	var stack []level
	var out strings.Builder
	sep := func() {
		if len(stack) == 0 {
			return
		}
		l := &stack[len(stack)-1]
		if l.afterKey {
			l.afterKey = false
			return
		}
		if l.count > 0 {
			out.WriteByte(',')
		}
		l.count++
	}
	for {
		tok, err := dec.ReadToken()
		if err == io.EOF {
			break
		}
		if err != nil {
			panic(err)
		}
		switch k := tok.Kind(); k {
		case '{', '[':
			sep()
			out.WriteByte(byte(k))
			stack = append(stack, level{object: k == '{'})
		case '}', ']':
			stack = stack[:len(stack)-1]
			out.WriteByte(byte(k))
		case '"':
			b, err := jsonv2.Marshal(tok.String())
			if err != nil {
				panic(err)
			}
			if len(stack) > 0 && stack[len(stack)-1].object && !stack[len(stack)-1].afterKey {
				sep()
				out.Write(b)
				out.WriteByte(':')
				stack[len(stack)-1].afterKey = true
			} else {
				sep()
				out.Write(b)
			}
		case '0':
			sep()
			out.WriteString(canonicalNumber(tok.String()))
		default:
			sep()
			out.WriteString(tok.String())
		}
	}
	return out.String(), true
}

func tokens(data []byte) (uint64, bool) {
	if !jsontext.Value(data).IsValid() {
		return 0, false
	}
	dec := jsontext.NewDecoder(bytes.NewReader(data))
	type level struct{ object, afterKey bool }
	var stack []level
	h := &fnv{0xcbf29ce484222325}
	for {
		tok, err := dec.ReadToken()
		if err == io.EOF {
			break
		}
		if err != nil {
			panic(err)
		}
		inKey := false
		if len(stack) > 0 && stack[len(stack)-1].object {
			l := &stack[len(stack)-1]
			if l.afterKey {
				l.afterKey = false
			} else if tok.Kind() == '"' {
				inKey = true
				l.afterKey = true
			}
		}
		var letter byte
		switch k := tok.Kind(); k {
		case '{', '[':
			letter = byte(k)
			stack = append(stack, level{object: k == '{'})
		case '}', ']':
			letter = byte(k)
			stack = stack[:len(stack)-1]
		case '"':
			letter = 's'
			if inKey {
				letter = 'k'
			}
		case '0':
			letter = 'n'
		case 't':
			letter = 't'
		case 'f':
			letter = 'f'
		case 'n':
			letter = 'z'
		}
		h.add([]byte{letter})
		if letter == '{' || letter == '}' || letter == '[' || letter == ']' {
			h.add([]byte{letter})
		} else {
			h.add([]byte(tok.String()))
		}
		h.add([]byte{'\n'})
	}
	return h.h, true
}

var pieces = []string{"{", "}", "[", "]", ",", ":", "\"", "\\", "\\u", "\\ud800", "\\udc00", "\\ud83d\\ude00", "\\u00e9", "0", "-", ".", "e", "E+", "1e400", "-0", "1.5e-10",
	"18446744073709551616", "true", "false", "null", " ", "\n", "\t", "\r", "\xff", "\xc3", "\xc3\xa9", "\xed\xa0\x80", "\xf0\x9f\x98\x80", "\x00", "\x1f", "\x7f",
	"\"a\":1", "\"a\":", ",\"a\":2", "[[[[", "]]]]", "{\"\":{}}", "\"x\"", "123456789", "0.1", " ", "\xef\xbb\xbf"}

func mutate(r *rand.Rand, s []byte, seeds [][]byte) []byte {
	out := append([]byte(nil), s...)
	n := 1
	if r.Intn(4) == 0 {
		n += r.Intn(3)
	}
	for ; n > 0; n-- {
		switch r.Intn(7) {
		case 0: // a byte replaced
			if len(out) > 0 {
				out[r.Intn(len(out))] = byte(r.Intn(256))
			}
		case 1, 2: // a piece inserted
			p := pieces[r.Intn(len(pieces))]
			at := r.Intn(len(out) + 1)
			out = append(out[:at], append([]byte(p), out[at:]...)...)
		case 3: // a range deleted
			if len(out) > 0 {
				a := r.Intn(len(out))
				b := a + r.Intn(min(8, len(out)-a)+1)
				out = append(out[:a], out[b:]...)
			}
		case 4: // a range repeated
			if len(out) > 0 {
				a := r.Intn(len(out))
				b := a + r.Intn(min(16, len(out)-a)+1)
				at := r.Intn(len(out) + 1)
				rep := append([]byte(nil), out[a:b]...)
				out = append(out[:at], append(rep, out[at:]...)...)
			}
		case 5: // cut short
			if len(out) > 0 {
				out = out[:r.Intn(len(out)+1)]
			}
		case 6: // spliced with another seed
			o := seeds[r.Intn(len(seeds))]
			if len(o) > 0 && len(out) > 0 {
				a := r.Intn(len(out) + 1)
				b := r.Intn(len(o) + 1)
				out = append(append([]byte(nil), out[:a]...), o[b:]...)
			}
		}
	}
	return out
}

// A random value of JSON: nested arrays and objects, numbers of every
// form, strings with escapes and characters of every width
func value(r *rand.Rand, depth int, sb *strings.Builder) {
	space := func() {
		if r.Intn(4) == 0 {
			sb.WriteString([]string{" ", "\n", "\t", "\r\n  "}[r.Intn(4)])
		}
	}
	space()
	k := r.Intn(10)
	if depth > 6 && k < 4 {
		k += 4
	}
	switch k {
	case 0, 1:
		sb.WriteByte('[')
		for i, n := 0, r.Intn(5); i < n; i++ {
			if i > 0 {
				sb.WriteByte(',')
			}
			value(r, depth+1, sb)
		}
		space()
		sb.WriteByte(']')
	case 2, 3:
		sb.WriteByte('{')
		n := r.Intn(5)
		if r.Intn(20) == 0 {
			n = 17 + r.Intn(10)
		}
		for i := 0; i < n; i++ {
			if i > 0 {
				sb.WriteByte(',')
			}
			space()
			sb.WriteString("\"k" + strconv.Itoa(i) + "\"")
			space()
			sb.WriteByte(':')
			value(r, depth+1, sb)
		}
		space()
		sb.WriteByte('}')
	case 4, 5:
		sb.WriteByte('"')
		for i, n := 0, r.Intn(12); i < n; i++ {
			sb.WriteString([]string{"a", "Z", " ", "\\n", "\\\"", "\\\\", "\\/", "\\u0041", "\\u00e9", "\\ud83d\\ude00", "\xc5\xbc", "\xe2\x82\xac", "\xf0\x9f\x98\x80", "\x7f", "\\b", "\\u2028"}[r.Intn(16)])
		}
		sb.WriteByte('"')
	case 6, 7:
		var lit strings.Builder
		if r.Intn(2) == 0 {
			lit.WriteByte('-')
		}
		switch r.Intn(5) {
		case 0:
			lit.WriteString(strconv.FormatUint(r.Uint64(), 10))
		case 1:
			lit.WriteString(strconv.Itoa(r.Intn(1000)))
		case 2:
			lit.WriteString(strconv.FormatFloat(r.NormFloat64()*math.Pow(10, float64(r.Intn(40)-20)), 'g', -1, 64))
		case 3:
			lit.WriteString(strconv.Itoa(r.Intn(100)) + "." + strconv.Itoa(r.Intn(100000)) + "e" + strconv.Itoa(r.Intn(40)-20))
		default:
			lit.WriteString(strconv.FormatFloat(math.Float64frombits(r.Uint64()>>2), 'e', -1, 64))
		}
		sb.WriteString(lit.String())
	case 8:
		sb.WriteString([]string{"true", "false", "null"}[r.Intn(3)])
	default:
		sb.WriteString(strconv.Itoa(r.Intn(10)))
	}
	space()
}

func main() {
	count, _ := strconv.Atoi(os.Args[1])
	seed, _ := strconv.ParseInt(os.Args[2], 10, 64)
	home, _ := os.UserHomeDir()
	dir := filepath.Join(home, "Programming", "oracles", "JSONTestSuite", "test_parsing")
	entries, err := os.ReadDir(dir)
	if err != nil {
		panic(err)
	}
	var seeds [][]byte
	for _, e := range entries {
		data, err := os.ReadFile(filepath.Join(dir, e.Name()))
		if err == nil && len(data) <= 4096 {
			seeds = append(seeds, data)
		}
	}
	seeds = append(seeds,
		[]byte(`{"a": [1, 2.5, -0, 1e-7, 1e21, 18446744073709551615, "xé\n"], "b": {"c": {"d": [true, false, null, {}, []]}}, "e": "😀"}`),
		[]byte(`[{"k1":1,"k2":2,"k3":3,"k4":4,"k5":5,"k6":6,"k7":7,"k8":8,"k9":9,"k10":10,"k11":11,"k12":12,"k13":13,"k14":14,"k15":15,"k16":16,"k17":17,"k18":18}]`),
		[]byte("\"za\xc5\xbc\xc3\xb3\xc5\x82\xc4\x87 g\xc4\x99\xc5\x9bl\xc4\x85 ja\xc5\xba\xc5\x84 \xe2\x82\xac \xf0\x9f\x98\x80\""),
		[]byte(`[0.1, 0.30000000000000004, 123456789.123456789, 5e-324, 1.7976931348623157e308, -1.5e-300, 100000000000000000000000]`),
	)
	r := rand.New(rand.NewSource(seed))
	w := os.Stdout
	var b8 [8]byte
	var b4 [4]byte
	for i := 0; i < count; i++ {
		var text []byte
		switch {
		case i < len(seeds):
			text = seeds[i]
		case i%3 == 0:
			var sb strings.Builder
			value(r, 0, &sb)
			text = []byte(sb.String())
			if r.Intn(3) == 0 {
				text = mutate(r, text, seeds)
			}
		default:
			text = mutate(r, seeds[r.Intn(len(seeds))], seeds)
		}
		th, valid := tokens(text)
		c, parses := canonical(text)
		binary.LittleEndian.PutUint32(b4[:], uint32(len(text)))
		w.Write(b4[:])
		w.Write(text)
		var flags byte
		if valid {
			flags |= 1
		}
		if parses {
			flags |= 2
		}
		w.Write([]byte{flags})
		binary.LittleEndian.PutUint64(b8[:], th)
		w.Write(b8[:])
		binary.LittleEndian.PutUint32(b4[:], uint32(len(c)))
		w.Write(b4[:])
		w.Write([]byte(c))
	}
}
