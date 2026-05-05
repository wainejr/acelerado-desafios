#include <fftw3.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

// Mathematical constants fallback (M_PI is POSIX, not strict C++ standard)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

const int HEIGHT = 512;
const int WIDTH = 512;
const int CHANNELS = 3;
const int HEADER_SIZE = 54;
const int PIXELS_COUNT = HEIGHT * WIDTH;
const int FFT_WIDTH = WIDTH / 2 + 1;

int main() {
  // Optimize standard I/O operations for performance
  std::ios_base::sync_with_stdio(false);
  std::cin.tie(NULL);

  // 1. Read BMP header and pixel data from standard input
  std::vector<char> bmp_header(HEADER_SIZE);
  if (!std::cin.read(bmp_header.data(), HEADER_SIZE)) return 0;

  std::vector<uint8_t> pixel_bytes(PIXELS_COUNT * CHANNELS);
  if (!std::cin.read(reinterpret_cast<char*>(pixel_bytes.data()),
                     pixel_bytes.size()))
    return 0;

  // 2. Pre-process the image
  // We use fftwf_ variables and functions for single-precision performance.
  float* image = (float*)fftwf_malloc(sizeof(float) * PIXELS_COUNT);
  float sum_img = 0.0f;

  for (int i = 0; i < PIXELS_COUNT; ++i) {
    // Extract one channel (assuming the target is a grayscale transformation)
    image[i] = static_cast<float>(pixel_bytes[i * CHANNELS]);
    sum_img += image[i];
  }
  float mean_img = sum_img / PIXELS_COUNT;

  // 3. Perform Forward Fast Fourier Transform (Spatial to Frequency Domain)
  fftwf_complex* image_fft =
    (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * HEIGHT * FFT_WIDTH);

  fftwf_plan plan_forward =
    fftwf_plan_dft_r2c_2d(HEIGHT, WIDTH, image, image_fft, FFTW_ESTIMATE);
  fftwf_execute(plan_forward);

  // 4. Estimate Blur (Gaussian Point Spread Function) from Frequency Spectrum
  float sum_x = 0.0f, sum_y = 0.0f, sum_x2 = 0.0f, sum_xy = 0.0f;
  int n_valid = 0;

  std::vector<float> freq_sq(HEIGHT * FFT_WIDTH);
  std::vector<float> magnitude(HEIGHT * FFT_WIDTH);

  for (int y = 0; y < HEIGHT; ++y) {
    // Map Y frequencies: [0, 1/N, ..., 0.5, -0.5, ..., -1/N]
    float fy =
      (y <= HEIGHT / 2) ? (float)y / HEIGHT : (float)(y - HEIGHT) / HEIGHT;
    float fy2 = fy * fy;

    for (int x = 0; x < FFT_WIDTH; ++x) {
      // Map X frequencies for Real FFT: [0, 1/N, ..., 0.5]
      float fx = (float)x / WIDTH;

      int idx = y * FFT_WIDTH + x;
      float f_sq = (fx * fx) + fy2;
      freq_sq[idx] = f_sq;

      float real = image_fft[idx][0];
      float imag = image_fft[idx][1];
      float mag = std::sqrt(real * real + imag * imag) + 1e-8f;
      magnitude[idx] = mag;

      // Sample specific frequency ranges to estimate the spectral drop-off (slope)
      if (f_sq > 0.001f && f_sq < 0.05f) {
        float log_mag = std::log(mag);
        sum_x += f_sq;
        sum_y += log_mag;
        sum_x2 += f_sq * f_sq;
        sum_xy += f_sq * log_mag;
        n_valid++;
      }
    }
  }

  // Calculate blur radius (sigma) using linear regression on log-magnitude
  float slope = 0.0f;
  if (n_valid > 0) {
    float denom = n_valid * sum_x2 - sum_x * sum_x;
    if (std::abs(denom) > 1e-8f)
      slope = (n_valid * sum_xy - sum_x * sum_y) / denom;
  }

  // Bound the estimated blur radius to prevent extreme artifacts
  float sigma = std::sqrt(std::max(0.0f, -2.0f * slope));
  sigma = std::clamp(sigma, 0.6f, 3.2f);

  // 5. Estimate Image Noise Variance using a Laplacian Operator
  float sum_lap = 0.0f;
  int count_lap = (HEIGHT - 2) * (WIDTH - 2);
  std::vector<float> laplacian(count_lap);
  int l_idx = 0;

  for (int y = 1; y < HEIGHT - 1; ++y) {
    for (int x = 1; x < WIDTH - 1; ++x) {
      // Discrete Laplacian filter to isolate high-frequency noise
      float res =
        image[y * WIDTH + x] -
        0.25f * (image[(y - 1) * WIDTH + x] + image[(y + 1) * WIDTH + x] +
                 image[y * WIDTH + (x - 1)] + image[y * WIDTH + (x + 1)]);
      laplacian[l_idx++] = res;
      sum_lap += res;
    }
  }

  float mean_lap = sum_lap / count_lap;
  float sq_sum_lap = 0.0f;
  for (float v : laplacian) sq_sum_lap += (v - mean_lap) * (v - mean_lap);
  float noise_var = sq_sum_lap / count_lap;

  // Estimate overall image variance
  float sq_sum_img = 0.0f;
  for (int i = 0; i < PIXELS_COUNT; ++i)
    sq_sum_img += (image[i] - mean_img) * (image[i] - mean_img);
  float sig_var = sq_sum_img / PIXELS_COUNT;

  // Calculate regularization strength (Inverse Signal-to-Noise Ratio proxy)
  float reg_strength = std::clamp(noise_var / (sig_var + 1e-6f), 1e-4f, 0.01f);

  // 6. Apply the Wiener Filter in the Frequency Domain
  const float alpha = 6.0f;  // Custom high-frequency weighting factor
  const float pi2_sigma2 = 2.0f * M_PI * M_PI * sigma * sigma;

  for (int i = 0; i < HEIGHT * FFT_WIDTH; ++i) {
    float f_sq = freq_sq[i];

    // Gaussian Point Spread Function (PSF) model
    float psf_f = std::exp(-pi2_sigma2 * f_sq);
    float psf_power = psf_f * psf_f;

    // Wiener filter denominator with custom regularization formulation
    float denominator =
      psf_power + reg_strength * (1.0f + alpha * (f_sq / (psf_power + 1e-6f)));

    image_fft[i][0] = (image_fft[i][0] * psf_f) / denominator;
    image_fft[i][1] = (image_fft[i][1] * psf_f) / denominator;
  }

  // 7. Perform Backward Fast Fourier Transform (Frequency to Spatial Domain)
  float* restored_raw = (float*)fftwf_malloc(sizeof(float) * PIXELS_COUNT);
  fftwf_plan plan_backward = fftwf_plan_dft_c2r_2d(HEIGHT, WIDTH, image_fft,
                                                   restored_raw, FFTW_ESTIMATE);
  fftwf_execute(plan_backward);

  // 8. Normalize Output and Write to Standard Output
  std::vector<uint8_t> output_rgb(PIXELS_COUNT * CHANNELS);
  float scale = 1.0f / PIXELS_COUNT;

  for (int i = 0; i < PIXELS_COUNT; ++i) {
    // Blend restored image with a small fraction of the original to retain structure
    float res = (restored_raw[i] * scale) * 0.998f + image[i] * 0.002f;
    uint8_t val = static_cast<uint8_t>(std::clamp(res, 0.0f, 255.0f));

    // Replicate grayscale values across RGB channels to maintain BMP format constraints
    output_rgb[i * 3] = output_rgb[i * 3 + 1] = output_rgb[i * 3 + 2] = val;
  }

  // Write reconstructed BMP out
  std::cout.write(bmp_header.data(), HEADER_SIZE);
  std::cout.write(reinterpret_cast<char*>(output_rgb.data()),
                  output_rgb.size());

  // 9. Cleanup and Memory Deallocation
  fftwf_destroy_plan(plan_forward);
  fftwf_destroy_plan(plan_backward);
  fftwf_free(image);
  fftwf_free(image_fft);
  fftwf_free(restored_raw);

  return 0;
}
