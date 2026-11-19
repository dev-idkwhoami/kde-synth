package main

import (
	"context"
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"strconv"
	"strings"
	"time"
)

type graphObject struct {
	ID   int    `json:"id"`
	Type string `json:"type"`
	Info struct {
		Props      map[string]any `json:"props"`
		OutputPort int            `json:"output-port-id"`
		InputPort  int            `json:"input-port-id"`
	} `json:"info"`
}

func prop(p map[string]any, key string) string {
	if p[key] == nil {
		return ""
	}
	return fmt.Sprint(p[key])
}
func executablePath(pid string) string {
	path, err := os.Readlink("/proc/" + pid + "/exe")
	if err != nil {
		return ""
	}
	return strings.TrimSuffix(path, " (deleted)")
}

type portPair struct{ Output, Input int }

func selectedLinks(objects []graphObject, executable, captureName string, resolve func(string) string) []portPair {
	clients := map[string]map[string]any{}
	selected := map[string]bool{}
	capture := ""
	for _, o := range objects {
		if o.Type == "PipeWire:Interface:Client" {
			clients[strconv.Itoa(o.ID)] = o.Info.Props
		}
	}
	for _, o := range objects {
		if o.Type != "PipeWire:Interface:Node" {
			continue
		}
		p := o.Info.Props
		if prop(p, "node.name") == captureName {
			capture = strconv.Itoa(o.ID)
		}
		if prop(p, "media.class") != "Stream/Output/Audio" {
			continue
		}
		pid := prop(p, "application.process.id")
		if pid == "" {
			pid = prop(clients[prop(p, "client.id")], "application.process.id")
		}
		// Fail closed when process identity cannot be verified.
		if pid != "" && resolve(pid) == executable {
			selected[strconv.Itoa(o.ID)] = true
		}
	}
	var outputs, inputs []graphObject
	for _, o := range objects {
		if o.Type != "PipeWire:Interface:Port" {
			continue
		}
		p := o.Info.Props
		if selected[prop(p, "node.id")] && prop(p, "port.direction") == "out" {
			outputs = append(outputs, o)
		}
		if capture != "" && prop(p, "node.id") == capture && prop(p, "port.direction") == "in" {
			inputs = append(inputs, o)
		}
	}
	var links []portPair
	for _, out := range outputs {
		for _, in := range inputs {
			channel := prop(out.Info.Props, "audio.channel")
			if channel == prop(in.Info.Props, "audio.channel") || channel == "MONO" {
				links = append(links, portPair{out.ID, in.ID})
			}
		}
	}
	return links
}
func routeApplication(ctx context.Context, executable, captureName string) {
	ticker := time.NewTicker(500 * time.Millisecond)
	defer ticker.Stop()
	for {
		scanCtx, cancel := context.WithTimeout(ctx, 2*time.Second)
		data, err := exec.CommandContext(scanCtx, "pw-dump").Output()
		cancel()
		var objects []graphObject
		if err == nil && json.Unmarshal(data, &objects) == nil {
			existing := map[portPair]bool{}
			for _, o := range objects {
				if o.Type == "PipeWire:Interface:Link" {
					existing[portPair{o.Info.OutputPort, o.Info.InputPort}] = true
				}
			}
			for _, pair := range selectedLinks(objects, executable, captureName, executablePath) {
				if existing[pair] {
					continue
				}
				linkCtx, stop := context.WithTimeout(ctx, time.Second)
				// These links only add a tap; the application's normal playback links stay intact.
				err := exec.CommandContext(linkCtx, "pw-link", "--passive", strconv.Itoa(pair.Output), strconv.Itoa(pair.Input)).Run()
				stop()
				if err != nil && ctx.Err() == nil {
					fmt.Fprintln(os.Stderr, "Application audio link unavailable:", err)
				}
			}
		}
		select {
		case <-ctx.Done():
			return
		case <-ticker.C:
		}
	}
}
