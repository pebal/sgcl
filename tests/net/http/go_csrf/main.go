// The other side of tests/net/http/middleware.cpp's CSRF cases: Go's
// http.CrossOriginProtection (Go 1.25 and later) with one trusted origin,
// https://trusted.example, around a handler that writes "ok", on a port
// of the loopback ("port N" on the first line); /quit ends the program.
package main

import (
	"fmt"
	"net"
	"net/http"
	"os"
)

func main() {
	p := http.NewCrossOriginProtection()
	if err := p.AddTrustedOrigin("https://trusted.example"); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	mux := http.NewServeMux()
	mux.HandleFunc("/quit", func(w http.ResponseWriter, r *http.Request) {
		os.Exit(0)
	})
	mux.Handle("/", p.Handler(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprint(w, "ok")
	})))
	l, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	fmt.Printf("port %d\n", l.Addr().(*net.TCPAddr).Port)
	os.Stdout.Sync()
	http.Serve(l, mux)
}
