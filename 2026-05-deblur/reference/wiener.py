#!/usr/bin/env python3
"""
Reference deblur solver — intentionally simple baseline.

Reads a 24-bit grayscale BMP from stdin (image blurred with a Gaussian PSF of
unknown sigma in [1.5, 3.5], plus white Gaussian noise sigma=2.0), applies
a Wiener filter with **fixed sigma = 2.5** (the midpoint of the range), and
writes the deblurred BMP to stdout.

This baseline does NOT estimate sigma. It's deliberately suboptimal: when the
true sigma is far from 2.5 the result is mediocre. The challenge is to do
better — estimate sigma adaptively, use a smarter filter (Richardson-Lucy,
TV-regularized, learning-based, ...), and beat the reference on time.
"""
from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
import bmp_io  # noqa: E402

SIGMA_FIXED = 2.5     # midpoint of the [1.5, 3.5] range
LAMBDA = 0.005        # Wiener regularization (tuned for noise sigma = 2.0)


def gaussian_psf_freq(shape: tuple[int, int], sigma: float) -> np.ndarray:
    h, w = shape
    yy = np.fft.fftfreq(h) * h
    xx = np.fft.fftfreq(w) * w
    yy, xx = np.meshgrid(yy, xx, indexing="ij")
    psf = np.exp(-(xx ** 2 + yy ** 2) / (2.0 * sigma ** 2))
    psf /= psf.sum()
    return np.fft.fft2(psf)


def wiener_deconvolve(blurred: np.ndarray, sigma: float, lam: float) -> np.ndarray:
    G = np.fft.fft2(blurred.astype(np.float64))
    H = gaussian_psf_freq(blurred.shape, sigma)
    F = G * np.conj(H) / (np.abs(H) ** 2 + lam)
    return np.real(np.fft.ifft2(F))


def main() -> None:
    blurred = bmp_io.read(sys.stdin.buffer)
    sharp = wiener_deconvolve(blurred, SIGMA_FIXED, LAMBDA)
    sharp = np.clip(sharp, 0, 255).astype(np.uint8)
    bmp_io.write(sys.stdout.buffer, sharp)


if __name__ == "__main__":
    main()
