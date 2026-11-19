package main

import (
	"bufio"
	"context"
	"encoding/binary"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"math"
	"os"
	"os/exec"
	"os/signal"
	"sync"
	"sync/atomic"
	"syscall"
	"time"
)

func run() error {
	hz := flag.Int("hz", 60, "Spectrum analyses per second (10–120)")
	gain := flag.Float64("gain", 1, "Fixed visualization gain (0.1–20)")
	executable := flag.String("app", "", "Capture only this executable; empty captures desktop audio")
	waveMs := flag.Int("wave-ms", 20, "Waveform history in milliseconds (5–200)")
	flag.Parse()
	if *waveMs < 5 || *waveMs > 200 {
		return fmt.Errorf("wave-ms must be 5–200")
	}
	if *hz < 10 || *hz > 120 {
		return fmt.Errorf("hz must be 10–120")
	}
	if !validGain(*gain) {
		return fmt.Errorf("gain must be finite and between 0.1 and 20")
	}
	ctx, cancel := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
	defer cancel()
	captureName := fmt.Sprintf("kde-synth-capture-%d", os.Getpid())
	properties := fmt.Sprintf("{ stream.capture.sink = true application.name = \"KDE Synth\" node.name = %s }", captureName)
	args := []string{"--raw", "--format", "f32", "--rate", "48000", "--channels", "2", "--latency", "256"}
	if *executable != "" {
		properties = fmt.Sprintf("{ node.autoconnect = false node.dont-reconnect = true node.passive = true application.name = \"KDE Synth\" node.name = %s }", captureName)
		args = append(args, "--target", "0")
	}
	args = append(args, "--properties", properties, "-")
	capture := exec.CommandContext(ctx, "pw-record", args...)
	capture.SysProcAttr = &syscall.SysProcAttr{Pdeathsig: syscall.SIGTERM}
	capture.Stderr = os.Stderr
	pipe, err := capture.StdoutPipe()
	if err != nil {
		return err
	}
	if err = capture.Start(); err != nil {
		return err
	}
	defer func() { cancel(); _ = capture.Wait() }()
	if *executable != "" {
		go routeApplication(ctx, *executable, captureName)
	}
	var mu sync.Mutex
	var left, right [historySize]float64
	position := 0
	var received time.Time
	failures := make(chan error, 1)
	go func() {
		block := make([]byte, 256*8)
		for {
			if _, err := io.ReadFull(pipe, block); err != nil {
				failures <- err
				return
			}
			mu.Lock()
			for i := 0; i < len(block); i += 8 {
				l := float64(math.Float32frombits(binary.LittleEndian.Uint32(block[i:])))
				r := float64(math.Float32frombits(binary.LittleEndian.Uint32(block[i+4:])))
				if math.IsNaN(l) || math.IsInf(l, 0) {
					l = 0
				}
				if math.IsNaN(r) || math.IsInf(r, 0) {
					r = 0
				}
				left[position], right[position] = l, r
				position = (position + 1) % historySize
			}
			received = time.Now()
			mu.Unlock()
		}
	}()
	var waveWindow atomic.Int64
	waveWindow.Store(int64(*waveMs))
	var gainBits atomic.Uint64
	gainBits.Store(math.Float64bits(*gain))
	rates := make(chan int, 1)
	go func() {
		scanner := bufio.NewScanner(os.Stdin)
		for scanner.Scan() {
			var control struct {
				WaveMs int      `json:"waveMs"`
				Hz     int      `json:"hz"`
				Gain   *float64 `json:"gain"`
			}
			if json.Unmarshal(scanner.Bytes(), &control) != nil {
				continue
			}
			if control.WaveMs >= 5 && control.WaveMs <= 200 {
				waveWindow.Store(int64(control.WaveMs))
			}
			if control.Gain != nil && validGain(*control.Gain) {
				gainBits.Store(math.Float64bits(*control.Gain))
			}
			if control.Hz >= 10 && control.Hz <= 120 {
				select {
				case rates <- control.Hz:
				default:
				}
			}
		}
		cancel() // Owner closed the control pipe; do not orphan the capture process.
	}()
	ticker := time.NewTicker(time.Second / time.Duration(*hz))
	defer ticker.Stop()
	encoder := json.NewEncoder(os.Stdout)
	for {
		select {
		case <-ctx.Done():
			return nil
		case err := <-failures:
			return fmt.Errorf("audio capture stopped: %w", err)
		case rate := <-rates:
			ticker.Reset(time.Second / time.Duration(rate))
		case <-ticker.C:
			l, r := make([]float64, fftSize), make([]float64, fftSize)
			mu.Lock()
			active := !received.IsZero() && time.Since(received) < 200*time.Millisecond
			if active {
				for i := range l {
					l[i], r[i] = left[(position-fftSize+i+historySize)%historySize], right[(position-fftSize+i+historySize)%historySize]
				}
			}
			count := int(waveWindow.Load()) * 48
			wl, wr := make([]float64, count), make([]float64, count)
			if active {
				for i := range wl {
					wl[i], wr[i] = left[(position-count+i+historySize)%historySize], right[(position-count+i+historySize)%historySize]
				}
			}
			mu.Unlock()
			trace := waveform(wl, wr, math.Float64frombits(gainBits.Load()))
			bands := make([]float64, bandCount)
			if active {
				applyGain(l, r, math.Float64frombits(gainBits.Load()))
				bands = spectrum(l, r)
			}
			if err := encoder.Encode(struct {
				Waveform  []float64 `json:"waveform"`
				Bands     []float64 `json:"bands"`
				Capturing bool      `json:"capturing"`
			}{trace, bands, active}); err != nil {
				return err
			}
		}
	}
}

func main() {
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}
