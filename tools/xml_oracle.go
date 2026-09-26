// SGCL: a C++20 application framework
// Copyright (c) 2022-2026 Sebastian Nibisz
// SPDX-License-Identifier: Apache-2.0
//
// The oracle for the tokens of sgcl/encoding/xml.h: Go's encoding/xml
// Decoder.Token (Strict, the default) over the same documents, each
// token written as the line tests/encoding/xml_common.h writes one, the
// answers as a C++ header the tests include. Run it from the root of the
// tree:
//
//     go run tools/xml_oracle.go > tests/encoding/xml_go_tests.h
//
// What is asked, by name, because an oracle checks only what it is asked:
//
//   - the documents of the list below, whole: the lines of their tokens
//     and whether Go read them to the end;
//   - every test of the W3C XML Conformance Test Suite the C++ test takes
//     (xml_conformance.cpp says which), when ~/Programming/oracles/xmlconf
//     holds it: whether Go read it to the end, and an FNV-1a hash of the
//     lines of its tokens.
//
// Where a line of Go and a line of this reader differ, and the tests
// know each by name:
//
//   - text next to text is one line on both sides (Go gives a CDATA
//     section as a token of its own, so does this reader; each splits
//     otherwise around comments it drops);
//   - the white space of an attribute's value: Go keeps a literal tab or
//     line feed, XML 1.0 3.3.3 makes it a space; a line writes both as a
//     space (the tests of values check the normalization); the same for a
//     namespace, which is the value of an xmlns attribute;
//   - a DOCTYPE declaration is "D name" on both sides, since Go gives its
//     whole text raw (a Directive) and this reader its parts;
//   - line endings in comments and instructions: Go keeps "\r\n" and "\r"
//     there, XML 1.0 2.11 makes every line ending of the input "\n" before
//     anything is read; the oracle does that to Go's comments and
//     instructions (Go does it to text itself);
//   - a byte order mark of UTF-8: Go gives it as text (and then reads an
//     XML declaration after it as a processing instruction), XML takes it
//     for the mark; the oracle gives Go the document without it.
//
// Not asked, because Go reads it wrong: an instruction holding '>' inside
// the internal subset of a DOCTYPE, where Go ends the declaration (the
// C++ tests have that document of their own; the fuzz test counts such
// documents apart).
//
// With -fuzz N the tool writes, in place of the header, N documents made by
// mutating the named ones and the suite's (bytes flipped, inserted, cut,
// doubled; two documents spliced), a line each: the document in hex,
// whether Go read it, the hash of its lines. The fuzz test of the C++ side
// (xml_go.cpp) reads that file when SGCL_XML_FUZZ names it:
//
//     go run tools/xml_oracle.go -fuzz 200000 > /some/scratch/xml_fuzz.txt
//
// Whether a document is well formed is not asked of Go: it reads a second
// root, text around the root, an unbound prefix and two attributes of one
// name, all of which XML refuses. The W3C suite is the oracle of that.
package main

import (
	"bytes"
	"encoding/hex"
	"encoding/xml"
	"fmt"
	"hash/fnv"
	"io"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
)

func shown(s string) string {
	var b strings.Builder
	b.WriteByte('"')
	for i := 0; i < len(s); i++ {
		c := s[i]
		if c < 0x20 || c == '"' || c == '\\' || c == 0x7F {
			fmt.Fprintf(&b, "\\x%02x", c)
		} else {
			b.WriteByte(c)
		}
	}
	b.WriteByte('"')
	return b.String()
}

func lines(s string) string {
	return strings.ReplaceAll(strings.ReplaceAll(s, "\r\n", "\n"), "\r", "\n")
}

func name(n xml.Name) string {
	return "{" + spaces(n.Space) + "}" + n.Local
}

func spaces(v string) string {
	return strings.Map(func(r rune) rune {
		if r == '\t' || r == '\n' || r == '\r' {
			return ' '
		}
		return r
	}, v)
}

// The lines of a document's tokens, and whether Go read it to its end
func dump(doc []byte) (string, bool) {
	d := xml.NewDecoder(bytes.NewReader(bytes.TrimPrefix(doc, []byte("\xEF\xBB\xBF"))))
	var out strings.Builder
	var pending strings.Builder
	flush := func() {
		if pending.Len() > 0 {
			out.WriteString("T " + shown(pending.String()) + "\n")
			pending.Reset()
		}
	}
	for {
		t, err := d.Token()
		if err != nil {
			flush()
			return out.String(), err == io.EOF
		}
		switch v := t.(type) {
		case xml.CharData:
			pending.Write(v)
			continue
		}
		flush()
		switch v := t.(type) {
		case xml.StartElement:
			out.WriteString("S " + name(v.Name))
			for _, a := range v.Attr {
				out.WriteString(" " + name(a.Name) + "=" + shown(spaces(a.Value)))
			}
			out.WriteString("\n")
		case xml.EndElement:
			out.WriteString("E " + name(v.Name) + "\n")
		case xml.Comment:
			out.WriteString("C " + shown(lines(string(v))) + "\n")
		case xml.ProcInst:
			out.WriteString("P " + v.Target + " " + shown(lines(string(v.Inst))) + "\n")
		case xml.Directive:
			f := strings.Fields(strings.TrimPrefix(string(v), "DOCTYPE"))
			n := ""
			if len(f) > 0 {
				n = strings.SplitN(strings.SplitN(f[0], "[", 2)[0], ">", 2)[0]
			}
			out.WriteString("D " + n + "\n")
		}
	}
}

func fnv64(s string) uint64 {
	h := fnv.New64a()
	h.Write([]byte(s))
	return h.Sum64()
}

func cpp(s string) string {
	var b strings.Builder
	b.WriteByte('"')
	for i := 0; i < len(s); i++ {
		c := s[i]
		switch {
		case c == '"' || c == '\\':
			b.WriteByte('\\')
			b.WriteByte(c)
		case c < 0x20 || c >= 0x7F:
			fmt.Fprintf(&b, "\\x%02x\"\"", c)
		default:
			b.WriteByte(c)
		}
	}
	b.WriteByte('"')
	return b.String()
}

// The documents asked by name: every kind of token, namespaces declared,
// undeclared and bound again, references, CDATA, line endings, names out
// of ASCII, quotes, a byte order mark, white space around the root
var named = []string{
	`<a/>`,
	`<?xml version="1.0"?><a b="1" c='2'>x</a>`,
	`<?xml version='1.0' encoding='UTF-8' standalone='yes' ?><a/>`,
	`<p:a xmlns:p="urn:p" xmlns="urn:d"><b p:x="1" y="2"/></p:a>`,
	`<a xmlns="urn:1" xmlns:p="urn:p"><b xmlns=""><p:c xmlns:p="urn:q"/><p:d/></b><e/></a>`,
	`<x:a xmlns:x="urn:x" xml:lang="pl"/>`,
	`<a>&lt;&gt;&amp;&apos;&quot;&#65;&#x42;&#x1F600;&#169;</a>`,
	`<a>x<![CDATA[<y>&amp;]]]]>z</a>`,
	`<!--c--><a><?pi data here?><?empty?></a><!--d-->`,
	"<a>1\r\n2\r3\n\r\n</a>",
	"<a>\r\n<!--x\r\ny-->\r\n<?p a\r\nb?></a>",
	`<!DOCTYPE a [<!ELEMENT a ANY><!-- ] > --><!ENTITY x "]>"> %pe;]><a/>`,
	`<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd"><html/>`,
	"<?xml version='1.0'?>\n<a/>\n",
	"<a  b = \"1\"\n\tc\r\n=\r\n'2' ></a >",
	"<\xC5\xBC\xC3\xB3\xC5\x82w \xC4\x85=\"\xC4\x99\"/>",
	`<a>]]</a>`,
	`<a b="'" c='"'>"'</a>`,
	`<a><!----></a>`,
	"<a x=\"1&#9;2\t3\n4\r\n5\r6&#10;7\"/>",
	`<root xmlns:h="http://www.w3.org/TR/html4/" xmlns:f="https://www.w3schools.com/furniture"><h:table><h:tr><h:td>Apples</h:td></h:tr></h:table><f:table><f:name>African Coffee Table</f:name><f:width>80</f:width></f:table></root>`,
	`<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="10"><use xlink:href="#a"/><text x="1">A &amp; B</text></svg>`,
	`<feed xmlns="http://www.w3.org/2005/Atom"><title type="text">T</title><entry><id>urn:1</id><content type="xhtml"><div xmlns="http://www.w3.org/1999/xhtml"><p>x<b>y</b>z</p></div></content></entry></feed>`,
	`<soap:Envelope xmlns:soap="http://www.w3.org/2003/05/soap-envelope" soap:encodingStyle="http://www.w3.org/2003/05/soap-encoding"><soap:Body><m:GetPrice xmlns:m="https://www.w3schools.com/prices"><m:Item>Apples</m:Item></m:GetPrice></soap:Body></soap:Envelope>`,
	"<a>\t tab and \xC2\xA0 nbsp \xE2\x80\xA8 ls</a>",
	`<a><b><c><d><e><f>deep</f></e></d></c></b></a>`,
	`<a b="&lt;&amp;&gt;&quot;&apos;"/>`,
	`<a>&#xD;&#xA;&#x9;&#32;</a>`,
	"<a><![CDATA[\r\n]]></a>",
	`<?target?><a/><?target data?>`,
	"<a xmlns:a='urn:a'><a:b a:c='1' c='2'/></a>",
	`<e:a xmlns:e="urn:e"><e:b/>text<![CDATA[cdata]]>more<!--c-->after</e:a>`,
}

// The structures of the typed test (xml_typed.cpp: WrittenAsGoWritesIt),
// with the value it writes
type Item struct {
	Sku   string `xml:"sku,attr"`
	Count int    `xml:"count,attr"`
	Gift  bool   `xml:"gift,attr"`
	Text  string `xml:",chardata"`
}

type Order struct {
	XMLName  xml.Name `xml:"order"`
	ID       int64    `xml:"id,attr"`
	Customer string   `xml:"customer"`
	Items    []Item   `xml:"item"`
	Notes    []string `xml:"note"`
	Total    uint32   `xml:"total"`
}

var typed = []any{
	Order{ID: -9007199254740993, Customer: "Kowalski & S\xC3\xB3n <sp. z o.o.>",
		Items: []Item{{"A-1", 2, true, "red \"big\" one"}, {"B'2", 0, false, ""}},
		Notes: []string{"fragile", ""}, Total: 4294967295},
}

type test struct {
	id, typ, file string
}

func conformance(root string) []test {
	catalogs := []string{
		"xmltest/xmltest.xml", "japanese/japanese.xml", "sun/sun-valid.xml", "sun/sun-invalid.xml", "sun/sun-not-wf.xml",
		"oasis/oasis.xml", "ibm/ibm_oasis_invalid.xml", "ibm/ibm_oasis_not-wf.xml", "ibm/ibm_oasis_valid.xml",
		"eduni/errata-2e/errata2e.xml", "eduni/errata-3e/errata3e.xml", "eduni/errata-4e/errata4e.xml",
		"eduni/namespaces/1.0/rmt-ns10.xml", "eduni/namespaces/errata-1e/errata1e.xml", "eduni/misc/ht-bh.xml",
	}
	var out []test
	for _, c := range catalogs {
		text, err := os.ReadFile(filepath.Join(root, c))
		if err != nil {
			continue
		}
		s := string(text)
		if strings.HasPrefix(s, "<?xml") {
			s = s[strings.Index(s, "?>")+2:]
		}
		d := xml.NewDecoder(strings.NewReader("<TESTCASES>" + s + "</TESTCASES>"))
		d.Strict = false
		dir := filepath.Dir(filepath.Join(root, c))
		for {
			t, err := d.Token()
			if err != nil {
				break
			}
			se, ok := t.(xml.StartElement)
			if !ok || se.Name.Local != "TEST" {
				continue
			}
			a := map[string]string{}
			for _, at := range se.Attr {
				a[at.Name.Local] = at.Value
			}
			if a["VERSION"] == "1.1" || a["RECOMMENDATION"] == "XML1.1" || a["RECOMMENDATION"] == "NS1.1" {
				continue
			}
			if e := a["EDITION"]; e != "" && !strings.Contains(e, "5") {
				continue
			}
			if a["NAMESPACE"] == "no" || a["TYPE"] == "error" {
				continue
			}
			out = append(out, test{a["ID"], a["TYPE"], filepath.Join(dir, a["URI"])})
		}
	}
	sort.Slice(out, func(i, j int) bool { return out[i].id < out[j].id })
	return out
}

// splitmix64, the generator of the mutations
type rng struct{ s uint64 }

func (r *rng) next() uint64 {
	r.s += 0x9E3779B97F4A7C15
	z := r.s
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9
	z = (z ^ (z >> 27)) * 0x94D049BB133111EB
	return z ^ (z >> 31)
}

func (r *rng) below(n int) int {
	return int(r.next() % uint64(n))
}

// What a mutation inserts: the characters of markup, and a few runs of it
var pieces = []string{"<", ">", "&", ";", "\"", "'", "/", "!", "?", "[", "]", "-", "=", " ", "\n", "\r", "\t", ":", "#", "x",
	"\xC5\xBC", "\xF0\x9F\x98\x80", "\x00", "\x01", "\xFF", "\xC3", "\xEF\xBF\xBE", "&amp;", "&lt;", "&#x41;", "&#0;", "&e;",
	"<a>", "</a>", "<a/>", "<!--", "-->", "<![CDATA[", "]]>", "<?p ", "?>", "xmlns:p='u'", "p:", "xmlns=''", " a='1'", "<!DOCTYPE d>"}

func mutate(r *rng, corpus [][]byte) []byte {
	d := append([]byte(nil), corpus[r.below(len(corpus))]...)
	for k := 1 + r.below(4); k > 0; k-- {
		n := len(d)
		switch r.below(6) {
		case 0:
			if n > 0 {
				d[r.below(n)] ^= byte(1 << r.below(8))
			}
		case 1:
			at := r.below(n + 1)
			p := pieces[r.below(len(pieces))]
			d = append(d[:at], append([]byte(p), d[at:]...)...)
		case 2:
			if n > 0 {
				at := r.below(n)
				l := 1 + r.below(8)
				if at+l > n {
					l = n - at
				}
				d = append(d[:at], d[at+l:]...)
			}
		case 3:
			if n > 0 {
				at := r.below(n)
				l := 1 + r.below(16)
				if at+l > n {
					l = n - at
				}
				d = append(d[:at+l], append(append([]byte(nil), d[at:at+l]...), d[at+l:]...)...)
			}
		case 4:
			if n > 0 {
				d[r.below(n)] = pieces[r.below(len(pieces))][0]
			}
		case 5:
			o := corpus[r.below(len(corpus))]
			a, b := r.below(n+1), r.below(len(o)+1)
			d = append(append([]byte(nil), d[:a]...), o[b:]...)
		}
	}
	return d
}

func fuzz(n int) {
	var corpus [][]byte
	for _, d := range named {
		corpus = append(corpus, []byte(d))
	}
	home, _ := os.UserHomeDir()
	for _, t := range conformance(filepath.Join(home, "Programming/oracles/xmlconf/xmlconf")) {
		if doc, err := os.ReadFile(t.file); err == nil && len(doc) < 2048 {
			corpus = append(corpus, doc)
		}
	}
	r := rng{12345}
	for i := 0; i < n; i++ {
		d := mutate(&r, corpus)
		tokens, ok := dump(d)
		fmt.Printf("%s %v %d\n", hex.EncodeToString(d), ok, fnv64(tokens))
	}
}

func main() {
	if len(os.Args) == 3 && os.Args[1] == "-fuzz" {
		n, _ := strconv.Atoi(os.Args[2])
		fuzz(n)
		return
	}
	fmt.Println("// Made by tools/xml_oracle.go (Go's encoding/xml as the oracle of the tokens); do not edit.")
	fmt.Println("#pragma once")
	fmt.Println()
	fmt.Println("#include <cstdint>")
	fmt.Println()
	fmt.Println("namespace {")
	fmt.Println("    struct GoDocument {")
	fmt.Println("        const char* doc;")
	fmt.Println("        bool ok;")
	fmt.Println("        const char* tokens;")
	fmt.Println("    };")
	fmt.Println()
	fmt.Println("    const GoDocument GoNamed[] = {")
	for _, d := range named {
		tokens, ok := dump([]byte(d))
		fmt.Printf("        {%s, %v,\n         %s},\n", cpp(d), ok, cpp(tokens))
	}
	fmt.Println("    };")
	fmt.Println()
	fmt.Println("    // xml.Marshal of the structures of the typed test")
	fmt.Println("    const char* const GoTyped[] = {")
	for _, v := range typed {
		b, err := xml.Marshal(v)
		if err != nil {
			panic(err)
		}
		fmt.Printf("        %s,\n", cpp(string(b)))
	}
	fmt.Println("    };")
	fmt.Println()
	fmt.Println("    struct GoConformance {")
	fmt.Println("        const char* id;")
	fmt.Println("        bool ok;")
	fmt.Println("        uint64_t tokens;   // FNV-1a of the lines")
	fmt.Println("    };")
	fmt.Println()
	fmt.Println("    const GoConformance GoSuite[] = {")
	home, _ := os.UserHomeDir()
	for _, t := range conformance(filepath.Join(home, "Programming/oracles/xmlconf/xmlconf")) {
		doc, err := os.ReadFile(t.file)
		if err != nil {
			continue
		}
		tokens, ok := dump(doc)
		fmt.Printf("        {%s, %v, 0x%016xull},\n", cpp(t.id), ok, fnv64(tokens))
	}
	fmt.Println("        {nullptr, false, 0},")
	fmt.Println("    };")
	fmt.Println("}")
}
