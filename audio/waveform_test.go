package main

import (
	"math"
	"testing"
)

func TestWaveform(t *testing.T) {
	left, right := make([]float64, 960), make([]float64, 960)
	for i := range left {
		left[i] = .25 * math.Sin(float64(i)*.1)
		right[i] = -left[i]
	}
	got := waveform(left, right, 1)
	peak := 0.0
	for _, v := range got {
		peak = math.Max(peak, math.Abs(v))
	}
	if len(got) != 512 || peak < .2 || peak > .26 {
		t.Fatalf("phase cancellation or wrong amplitude: %f", peak)
	}
	boosted := waveform(left, right, 2)
	for i, v := range got {
		if math.Abs(boosted[i]-2*v) > 1e-10 {
			t.Fatal("gain mismatch")
		}
	}
	for _, v := range waveform(make([]float64, 240), make([]float64, 240), 20) {
		if v != 0 {
			t.Fatal("silence")
		}
	}
	if len(waveform(nil, nil, 1)) != 512 {
		t.Fatal("missing waveform shape")
	}
}

func TestWaveformPreservesFastPeaks(t *testing.T) {
	left, right := make([]float64, 9600), make([]float64, 9600)
	for i := range left {
		left[i] = .2
		if i%2 == 0 {
			left[i] = -.2
		}
		right[i] = left[i]
	}
	wave := waveform(left, right, 1)
	for i := 0; i < len(wave); i += 2 {
		if math.Abs(wave[i]) != .2 || math.Abs(wave[i+1]) != .2 || wave[i]*wave[i+1] >= 0 {
			t.Fatalf("lost extrema in bucket %d", i/2)
		}
	}
	// Short windows interpolate the original endpoints rather than dropping them.
	short := waveform([]float64{-.5, .5}, []float64{0, 0}, 1)
	if short[0] != -.5 || short[511] != .5 {
		t.Fatal("short-window endpoints changed")
	}
}
