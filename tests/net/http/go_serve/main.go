// The other side of tests/net/http/serve_content.cpp: Go's
// http.ServeContent over the same bytes and validators as the module's
// serve_content, on a port of the loopback ("port N" on the first line).
// /c serves 4000 bytes ("0123456789abcdefghij" repeated) with the ETag
// of the query's etag= (none when absent) and Last-Modified
// 2023-11-14 22:13:20 UTC; /quit ends the program.
package main

import (
	"bytes"
	"fmt"
	"net"
	"net/http"
	"os"
	"strings"
	"time"
)

func main() {
	content := []byte(strings.Repeat("0123456789abcdefghij", 200))
	mod := time.Unix(1700000000, 0).UTC()
	mux := http.NewServeMux()
	mux.HandleFunc("/c", func(w http.ResponseWriter, r *http.Request) {
		if e := r.URL.Query().Get("etag"); e != "" {
			w.Header().Set("ETag", e)
		}
		http.ServeContent(w, r, "c.txt", mod, bytes.NewReader(content))
	})
	mux.HandleFunc("/quit", func(w http.ResponseWriter, r *http.Request) {
		os.Exit(0)
	})
	l, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	fmt.Printf("port %d\n", l.Addr().(*net.TCPAddr).Port)
	os.Stdout.Sync()
	http.Serve(l, mux)
}
