package main

import (
	"math"
	"testing"
)

func TestSilence(t *testing.T) {
	for _, v := range spectrum(make([]float64, fftSize), make([]float64, fftSize)) {
		if v != 0 {
			t.Fatal("silence generated energy")
		}
	}
}
func TestToneAndStereoCancellation(t *testing.T) {
	for _, frequency := range []float64{100, 1000, 8000} {
		l, r := make([]float64, fftSize), make([]float64, fftSize)
		for i := range l {
			l[i] = 0.5 * math.Sin(2*math.Pi*frequency*float64(i)/sampleRate)
			r[i] = -l[i]
		}
		bands := spectrum(l, r)
		peak := 0
		for i, v := range bands {
			if v > bands[peak] {
				peak = i
			}
		}
		center := 30 * math.Pow(16000.0/30, (float64(peak)+0.5)/bandCount)
		if math.Abs(math.Log(center/frequency)) > 0.20 || bands[peak] < 0.8 {
			t.Fatalf("%g Hz: peak %g Hz at %g", frequency, center, bands[peak])
		}
	}
}

func TestFixedGain(t *testing.T) {
	makeTone := func() ([]float64, []float64) {
		l, r := make([]float64, fftSize), make([]float64, fftSize)
		for i := range l {
			l[i] = .01 * math.Sin(2*math.Pi*3000*float64(i)/sampleRate)
			r[i] = -l[i] * .6
		}
		return l, r
	}
	l, r := makeTone()
	original := spectrum(l, r)
	applyGain(l, r, 1)
	same := spectrum(l, r)
	for i := range same {
		if same[i] != original[i] {
			t.Fatal("unity gain changed spectrum")
		}
	}
	applyGain(l, r, 10)
	boosted := spectrum(l, r)
	peak := 0
	for i := range original {
		if original[i] > original[peak] {
			peak = i
		}
	}
	if math.Abs(boosted[peak]-original[peak]-20.0/70) > .001 {
		t.Fatal("10x should add 20 dB")
	}
	for i := 0; i < 33; i++ {
		if boosted[i] > .01 {
			t.Fatalf("high tone triggered bass band %d: %g", i, boosted[i])
		}
	}
	silence := make([]float64, fftSize)
	applyGain(silence, silence, 20)
	for _, v := range spectrum(silence, silence) {
		if v != 0 {
			t.Fatal("gain amplified silence")
		}
	}
	for _, bad := range []float64{0, -1, 21, math.NaN(), math.Inf(1)} {
		if validGain(bad) {
			t.Fatal("invalid gain accepted")
		}
	}
}
