package main

import "math"

const historySize = 16384
const waveformPoints = 512

// Preserve both extrema in each time bucket, in chronological order.
// Averaging would cancel fast oscillations and flatten real music.
// Use the louder channel so opposite-phase stereo doesn't cancel the trace.
func waveform(left, right []float64, gain float64) []float64 {
	out := make([]float64, waveformPoints)
	if len(left) == 0 || len(left) != len(right) {
		return out
	}
	le, re := 0.0, 0.0
	for i := range left {
		le += left[i] * left[i]
		re += right[i] * right[i]
	}
	samples := left
	if re > le {
		samples = right
	}
	clamp := func(v float64) float64 { return math.Max(-1, math.Min(1, v*gain)) }
	if len(samples) <= waveformPoints {
		for i := range out {
			at := float64(i) * float64(len(samples)-1) / float64(waveformPoints-1)
			lo := int(at)
			hi := min(len(samples)-1, lo+1)
			fraction := at - float64(lo)
			out[i] = clamp(samples[lo]*(1-fraction) + samples[hi]*fraction)
		}
		return out
	}
	for bucket := 0; bucket < waveformPoints/2; bucket++ {
		start := bucket * len(samples) / (waveformPoints / 2)
		end := (bucket + 1) * len(samples) / (waveformPoints / 2)
		low, high := start, start
		for i := start + 1; i < end; i++ {
			if samples[i] < samples[low] {
				low = i
			}
			if samples[i] > samples[high] {
				high = i
			}
		}
		first, last := low, high
		if first > last {
			first, last = last, first
		}
		out[bucket*2], out[bucket*2+1] = clamp(samples[first]), clamp(samples[last])
	}
	return out
}
