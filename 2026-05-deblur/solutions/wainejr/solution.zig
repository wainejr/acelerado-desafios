// Exemplo passthrough: copia stdin pra stdout sem alterar nada.
const std = @import("std");

const SIZE_X = 512;
const SIZE_Y = 512;

const BMP_PIXEL_BYTES = 0x3;
const BMP_HEADER_SIZE = 0x36;
var BMP_HEADER: [BMP_HEADER_SIZE]u8 = undefined;

const IMG_SIZE = SIZE_X * SIZE_Y * BMP_PIXEL_BYTES;
var IMG_BUFFER: [SIZE_X * SIZE_Y * BMP_PIXEL_BYTES]u8 = undefined;
var IMG_SCRATCH_BUFFER: [SIZE_X * SIZE_Y]u8 = undefined;
const IMG_CHUNK_SIZE = 512;

inline fn idx2pos(idx: usize) struct { x: i16, y: i16 } {
    return .{ .x = @as(i16, @intCast(idx % SIZE_X)), .y = @as(i16, @intCast(idx / SIZE_X)) };
}

const IdxError = error{};

inline fn pos2idx(pos: struct { x: i16, y: i16 }) IdxError!usize {
    if ((pos.x < 0) || (pos.y < 0)) {
        return IdxError;
    }
    var idx: usize = @intCast(pos.x);
    idx += @as(usize, @intCast(pos.y)) * SIZE_X;
    return idx;
}

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
fn img_noise_reducer() !void {
    for (0..IMG_SIZE / 3) |idx| {
        const pos = idx2pos(idx);

        var n_samples: usize = 0;
        var samples: [9]u8 = undefined;
        for (0..3) |iy| {
            for (0..3) |ix| {
                const pi = .{ .x = pos.x - 1 - @as(i16, @intCast(ix)), .y = pos.y - 1 - @as(i16, @intCast(iy)) };

                const pidx = pos2idx(pi) catch {
                    break;
                };
                const v = IMG_BUFFER[pidx * BMP_PIXEL_BYTES];

                var idx_insert: usize = 0;
                for (0..n_samples) |iv| {
                    if (samples[iv] > iv) {
                        idx_insert = iv;
                        break;
                    }
                }

                if (idx_insert < n_samples)
                    std.mem.copyBackwards(u8, samples[idx_insert + 1 .. n_samples + 1], samples[idx_insert..n_samples]);
                samples[idx_insert] = v;
                n_samples += 1;
            }
        }
        const median = samples[n_samples / 2];
        IMG_SCRATCH_BUFFER[idx] = median;
    }
}

pub fn main() !void {
    try io_img(std.posix.read, std.posix.STDIN_FILENO);

    try img_noise_reducer();

    try img_fft();
    try img_sigma_estimator();
    try img_deblur_filter();
    try img_ifft();

    try io_img(std.posix.write, std.posix.STDOUT_FILENO);
}
