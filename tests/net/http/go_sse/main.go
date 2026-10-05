// Server-Sent Events in Go for the interop tests, with the standard library
// alone (net/http and its Flusher; the reading written by hand from the
// WHATWG HTML Standard §9.2.6):
//
//	go_sse server        an event stream at /events: four events (a type, an id, data of two
//	                     lines, a retry, a comment), resumed after Last-Event-ID; the stream at
//	                     /cr is the same with CR line ends; prints "port N"
//	go_sse client URL    the events of URL, one line each, "type|id|data" (LF in data as \n),
//	                     until the stream ends
//
// The server ends at a request for /quit, or after two minutes.
package main

import (
	"bufio"
	"fmt"
	"net"
	"net/http"
	"os"
	"strconv"
	"strings"
	"time"
)

func server() {
	l, err := net.Listen("tcp", "127.0.0.1:0")
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	fmt.Printf("port %d\n", l.Addr().(*net.TCPAddr).Port)
	os.Stdout.Sync()
	go func() {
		time.Sleep(2 * time.Minute)
		os.Exit(0)
	}()
	stream := func(nl string) http.HandlerFunc {
		return func(w http.ResponseWriter, r *http.Request) {
			w.Header().Set("Content-Type", "text/event-stream")
			w.Header().Set("Cache-Control", "no-cache")
			f := w.(http.Flusher)
			from := 0
			if id := r.Header.Get("Last-Event-ID"); id != "" {
				from, _ = strconv.Atoi(id)
				from++
			}
			fmt.Fprintf(w, ": hello from go%s%s", nl, nl)
			f.Flush()
			for i := from; i < 4; i++ {
				fmt.Fprintf(w, "event: tick%sid: %d%sretry: 50%sdata: first line %d%sdata: second line%s%s", nl, i, nl, nl, i, nl, nl, nl)
				f.Flush()
			}
		}
	}
	mux := http.NewServeMux()
	mux.HandleFunc("/events", stream("\n"))
	mux.HandleFunc("/cr", stream("\r"))
	mux.HandleFunc("/quit", func(w http.ResponseWriter, r *http.Request) { os.Exit(0) })
	(&http.Server{Handler: mux}).Serve(l)
}

func client(url string) {
	res, err := http.Get(url)
	if err != nil {
		fmt.Println("error:", err)
		return
	}
	defer res.Body.Close()
	if ct := res.Header.Get("Content-Type"); !strings.HasPrefix(ct, "text/event-stream") {
		fmt.Println("content-type:", ct)
		return
	}
	r := bufio.NewReader(res.Body)
	var data []string
	typ, id := "", ""
	for {
		line, err := r.ReadString('\n')
		if err != nil {
			return
		}
		line = strings.TrimSuffix(strings.TrimSuffix(line, "\n"), "\r")
		if line == "" {
			if data != nil {
				if typ == "" {
					typ = "message"
				}
				fmt.Printf("%s|%s|%s\n", typ, id, strings.ReplaceAll(strings.Join(data, "\n"), "\n", "\\n"))
			}
			data, typ = nil, ""
			continue
		}
		if strings.HasPrefix(line, ":") {
			continue
		}
		field, value, _ := strings.Cut(line, ":")
		value = strings.TrimPrefix(value, " ")
		switch field {
		case "data":
			data = append(data, value)
		case "event":
			typ = value
		case "id":
			id = value
		}
	}
}

func main() {
	switch {
	case len(os.Args) >= 2 && os.Args[1] == "server":
		server()
	case len(os.Args) >= 3 && os.Args[1] == "client":
		client(os.Args[2])
	default:
		fmt.Fprintln(os.Stderr, "usage: go_sse server | client URL")
		os.Exit(2)
	}
}
