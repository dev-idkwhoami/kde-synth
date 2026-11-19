package main

import (
	"math"
	"math/cmplx"
)

const sampleRate = 48000
const fftSize = 4096
const bandCount = 128

// Iterative radix-2 FFT. Both channels are analyzed separately so out-of-phase
// stereo cannot cancel the visualization.
func fft(a []complex128) {
	n := len(a)
	for i, j := 1, 0; i < n; i++ {
		bit := n >> 1
		for ; j&bit != 0; bit >>= 1 {
			j ^= bit
		}
		j ^= bit
		if i < j {
			a[i], a[j] = a[j], a[i]
		}
	}
	for length := 2; length <= n; length *= 2 {
		step := cmplx.Rect(1, -2*math.Pi/float64(length))
		for i := 0; i < n; i += length {
			w := complex(1, 0)
			for j := 0; j < length/2; j++ {
				u, v := a[i+j], a[i+j+length/2]*w
				a[i+j], a[i+j+length/2] = u+v, u-v
				w *= step
			}
		}
	}
}

func spectrum(left, right []float64) []float64 {
	l, r := make([]complex128, fftSize), make([]complex128, fftSize)
	for i := range l {
		window := 0.5 * (1 - math.Cos(2*math.Pi*float64(i)/float64(fftSize-1)))
		l[i], r[i] = complex(left[i]*window, 0), complex(right[i]*window, 0)
	}
	fft(l)
	fft(r)
	result := make([]float64, bandCount)
	for b := range result {
		low := 30 * math.Pow(16000.0/30, float64(b)/bandCount)
		high := 30 * math.Pow(16000.0/30, float64(b+1)/bandCount)
		first := max(1, int(math.Floor(low*fftSize/sampleRate)))
		last := min(fftSize/2, int(math.Ceil(high*fftSize/sampleRate)))
		peak := 0.0
		for k := first; k <= last; k++ {
			peak = math.Max(peak, math.Max(cmplx.Abs(l[k]), cmplx.Abs(r[k]))*4/fftSize)
		}
		if peak > 0 {
			result[b] = math.Max(0, math.Min(1, (20*math.Log10(peak)+70)/70))
		}
	}
	return result
}

// Fixed pre-FFT gain does not clip samples or generate new frequency content.
func validGain(gain float64) bool {
	return !math.IsNaN(gain) && !math.IsInf(gain, 0) && gain >= .1 && gain <= 20
}
func applyGain(left, right []float64, gain float64) {
	for i := range left {
		left[i] *= gain
		right[i] *= gain
	}
}
