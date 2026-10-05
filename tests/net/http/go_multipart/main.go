// Go's mime/multipart as the other side of the multipart tests (built by
// tests/net/http/multipart.cpp with `go build`; skipped where there is no
// go):
//
//	go_multipart write              a body written by multipart.Writer: its boundary on the
//	                                first line, the body after it
//	go_multipart read BOUNDARY      a body on stdin read by multipart.Reader: a line a part,
//	                                "name|filename|content" for a field, "name|filename|size"
//	                                for a file
package main

import (
	"bytes"
	"fmt"
	"io"
	"mime/multipart"
	"net/textproto"
	"os"
)

func write() {
	var b bytes.Buffer
	w := multipart.NewWriter(&b)
	w.WriteField("field", "value")
	w.WriteField("quote\"d", "with \"quotes\"")
	f, _ := w.CreateFormFile("file", "a.txt")
	f.Write([]byte("file content\r\n--" + w.Boundary() + "x\r\n"))
	h := make(textproto.MIMEHeader)
	h.Set("Content-Disposition", `form-data; name="big"; filename="big.bin"`)
	h.Set("Content-Type", "image/png")
	p, _ := w.CreatePart(h)
	big := make([]byte, 1000000-4)
	for i := range big {
		big[i] = byte(i * 7)
	}
	p.Write(big)
	w.Close()
	fmt.Println(w.Boundary())
	os.Stdout.Write(b.Bytes())
}

func read(boundary string) {
	r := multipart.NewReader(os.Stdin, boundary)
	for {
		p, err := r.NextPart()
		if err == io.EOF {
			return
		}
		if err != nil {
			fmt.Println("error:", err)
			return
		}
		data, err := io.ReadAll(p)
		if err != nil {
			fmt.Println("error:", err)
			return
		}
		if p.FileName() != "" {
			fmt.Printf("%s|%s|%d\n", p.FormName(), p.FileName(), len(data))
		} else {
			fmt.Printf("%s||%s\n", p.FormName(), data)
		}
	}
}

func main() {
	switch {
	case len(os.Args) >= 2 && os.Args[1] == "write":
		write()
	case len(os.Args) >= 3 && os.Args[1] == "read":
		read(os.Args[2])
	default:
		fmt.Fprintln(os.Stderr, "usage: go_multipart write | read BOUNDARY")
		os.Exit(2)
	}
}
