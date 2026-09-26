// Same shape as benchmarks/encoding/xml.cpp: Go's encoding/xml over the same
// document.
//   xml [op=tokens] [books=5000] [count]
// ops: tokens (Decoder.Token over a bytes.Reader), stream (the same over a
// reader of 4 KB reads), tree (Unmarshal into a node of any elements,
// attributes and chardata: Go has no tree), write (Marshal of those nodes),
// typed (Unmarshal into tagged structs).
// Prints nanoseconds per document and megabytes of it per second.
package main

import (
	"bytes"
	"encoding/xml"
	"fmt"
	"io"
	"os"
	"strconv"
	"time"
)

func document(books int) []byte {
	var d bytes.Buffer
	d.WriteString("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<catalog xmlns=\"urn:books\" xmlns:dc=\"http://purl.org/dc/elements/1.1/\">\n")
	for i := 0; i < books; i++ {
		n := strconv.Itoa(i)
		d.WriteString("  <book id=\"b" + n + "\" lang=\"pl\" available=\"true\">\n")
		d.WriteString("    <dc:title>Title number " + n + " &amp; more</dc:title>\n")
		d.WriteString("    <author first=\"Jan\" last=\"Kowalski\"/>\n")
		d.WriteString("    <price currency=\"PLN\">" + strconv.Itoa(10+i%90) + ".99</price>\n")
		d.WriteString("    <description>A book about &lt;things&gt;, number " + n + ", zażółć gęślą jaźń.</description>\n")
		d.WriteString("  </book>\n")
	}
	d.WriteString("</catalog>\n")
	return d.Bytes()
}

// A node of any element: what a program without a schema unmarshals into
type node struct {
	XMLName xml.Name
	Attrs   []xml.Attr `xml:",any,attr"`
	Text    string     `xml:",chardata"`
	Nodes   []node     `xml:",any"`
}

type Book struct {
	ID          string `xml:"id,attr"`
	Lang        string `xml:"lang,attr"`
	Available   bool   `xml:"available,attr"`
	Title       string `xml:"http://purl.org/dc/elements/1.1/ title"`
	Author      struct {
		First string `xml:"first,attr"`
		Last  string `xml:"last,attr"`
	} `xml:"author"`
	Price struct {
		Currency string  `xml:"currency,attr"`
		Amount   float64 `xml:",chardata"`
	} `xml:"price"`
	Description string `xml:"description"`
}

type Catalog struct {
	Books []Book `xml:"book"`
}

type pieces struct {
	b     []byte
	piece int
}

func (p *pieces) Read(out []byte) (int, error) {
	if len(p.b) == 0 {
		return 0, io.EOF
	}
	n := min(len(out), p.piece, len(p.b))
	copy(out, p.b[:n])
	p.b = p.b[n:]
	return n, nil
}

var sink int

func timed(count int, f func()) float64 {
	for i := 0; i < min(count, 3); i++ {
		f()
	}
	t0 := time.Now()
	for i := 0; i < count; i++ {
		f()
	}
	return float64(time.Since(t0).Nanoseconds()) / float64(count)
}

func tokens(r io.Reader) int {
	d := xml.NewDecoder(r)
	n := 0
	for {
		if _, err := d.Token(); err != nil {
			return n
		}
		n++
	}
}

func main() {
	op := "tokens"
	if len(os.Args) > 1 {
		op = os.Args[1]
	}
	books := 5000
	if len(os.Args) > 2 {
		books, _ = strconv.Atoi(os.Args[2])
	}
	doc := document(books)
	count := 2_000_000_000/5/len(doc) + 1
	if len(os.Args) > 3 {
		count, _ = strconv.Atoi(os.Args[3])
	}
	var ns float64
	switch op {
	case "tokens":
		ns = timed(count, func() { sink += tokens(bytes.NewReader(doc)) })
	case "stream":
		ns = timed(count, func() { sink += tokens(&pieces{doc, 4096}) })
	case "tree":
		ns = timed(count, func() {
			var n node
			if err := xml.Unmarshal(doc, &n); err != nil {
				panic(err)
			}
			sink += len(n.Nodes)
		})
	case "typed":
		ns = timed(count, func() {
			var c Catalog
			if err := xml.Unmarshal(doc, &c); err != nil {
				panic(err)
			}
			sink += len(c.Books)
		})
	case "write":
		var n node
		if err := xml.Unmarshal(doc, &n); err != nil {
			panic(err)
		}
		ns = timed(count, func() {
			b, err := xml.Marshal(&n)
			if err != nil {
				panic(err)
			}
			sink += len(b)
		})
	default:
		fmt.Fprintln(os.Stderr, "xml: no op called", op)
		os.Exit(2)
	}
	fmt.Printf("go op=%s books=%d bytes=%d count=%d ns/op=%.0f MB/s=%.0f\n", op, books, len(doc), count, ns, float64(len(doc))/ns*1e3)
}
