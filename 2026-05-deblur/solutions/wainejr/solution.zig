// Exemplo passthrough: copia stdin pra stdout sem alterar nada.
const std = @import("std");

const SIZE_X = 512;
const SIZE_Y = 512;

const BMP_PIXEL_BYTES = 0x3;
const BMP_HEADER_SIZE = 0x36;
var BMP_HEADER: [BMP_HEADER_SIZE]u8 = undefined;

const IMG_SIZE = SIZE_X * SIZE_Y * BMP_PIXEL_BYTES;
var IMG_BUFFER: [SIZE_X * SIZE_Y * BMP_PIXEL_BYTES]u8 = undefined;
const IMG_CHUNK_SIZE = 512;

fn io_img(func: anytype, fd: std.posix.fd_t) !void {
    _ = try func(fd, &BMP_HEADER);
    for (0..IMG_SIZE / IMG_CHUNK_SIZE) |i| {
        const view = IMG_BUFFER[i * IMG_CHUNK_SIZE .. (i + 1) * IMG_CHUNK_SIZE];
        var idx = try func(fd, view);
        if (idx == 0) break;
        while (idx < IMG_CHUNK_SIZE) {
            idx += try func(fd, view[idx..]);
        }
    }
}

/// Run FFT on img for processing
fn img_fft() !void {}

/// Run inverse FFT on img
fn img_ifft() !void {}

/// Estimate sigma of img
fn img_sigma_estimator() !void {}

/// Run deblur filter based on the estimations done
fn img_deblur_filter() !void {}

/// reduce white noise in image
fn img_noise_reducer() !void {}

pub fn main() !void {
    try io_img(std.posix.read, std.posix.STDIN_FILENO);

    try img_noise_reducer();

    try img_fft();
    try img_sigma_estimator();
    try img_deblur_filter();
    try img_ifft();

    try io_img(std.posix.write, std.posix.STDOUT_FILENO);
}
