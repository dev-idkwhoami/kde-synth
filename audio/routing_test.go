package main

import (
	"encoding/json"
	"testing"
)

func TestSelectedLinks(t *testing.T) {
	var graph []graphObject
	err := json.Unmarshal([]byte(`[
 {"id":1,"type":"PipeWire:Interface:Client","info":{"props":{"application.process.id":11}}},
 {"id":2,"type":"PipeWire:Interface:Node","info":{"props":{"client.id":1,"media.class":"Stream/Output/Audio"}}},
 {"id":3,"type":"PipeWire:Interface:Node","info":{"props":{"application.process.id":22,"media.class":"Stream/Output/Audio"}}},
 {"id":4,"type":"PipeWire:Interface:Node","info":{"props":{"node.name":"capture"}}},
 {"id":5,"type":"PipeWire:Interface:Port","info":{"props":{"node.id":2,"port.direction":"out","audio.channel":"FL"}}},
 {"id":6,"type":"PipeWire:Interface:Port","info":{"props":{"node.id":3,"port.direction":"out","audio.channel":"FL"}}},
 {"id":7,"type":"PipeWire:Interface:Port","info":{"props":{"node.id":4,"port.direction":"in","audio.channel":"FL"}}},
 {"id":8,"type":"PipeWire:Interface:Port","info":{"props":{"node.id":4,"port.direction":"in","audio.channel":"FR"}}}
 ]`), &graph)
	if err != nil {
		t.Fatal(err)
	}
	resolve := func(pid string) string {
		if pid == "11" {
			return "/app/selected"
		}
		return "/app/other"
	}
	links := selectedLinks(graph, "/app/selected", "capture", resolve)
	if len(links) != 1 || links[0] != (portPair{5, 7}) {
		t.Fatalf("wrong source: %v", links)
	}
	if len(selectedLinks(graph, "/app/missing", "capture", resolve)) != 0 {
		t.Fatal("must not fall back")
	}
	if len(selectedLinks(graph, "/app/selected", "capture", func(string) string { return "" })) != 0 {
		t.Fatal("unverified identity must not match")
	}
}
