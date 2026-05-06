// Exemplo passthrough: copia stdin pra stdout sem alterar nada.
const std = @import("std");

const SIZE_X = 512;
const SIZE_Y = 512;

const IMG_BUFFER: [512][512]u8 = undefined;

const BMP_HEADER_SIZE = 0x36;
var BMP_HEADER: u8[BMP_HEADER_SIZE] = undefined;

fn read_img() !void {
    try std.posix.read(std.posix.STDIN_FILENO, &BMP_HEADER);

    for (0..512 * 512) |i| {
        const ix = i % 512;
        const iy = i / 512;

        const PIXEL_BMP_BYTES = 0x4;
        var row: u8[PIXEL_BMP_BYTES] = undefined;
        try std.posix.read(std.posix.STDIN_FILENO, &row);
        IMG_BUFFER[ix][iy] = row[0];
    }
}

fn write_img() !void {
    try std.posix.write(std.posix.STDOUT_FILENO, BMP_HEADER);

    for (0..512 * 512) |i| {
        const ix = i % 512;
        const iy = i / 512;

        const PIXEL_BMP_BYTES = 0x4;
        const pixel = IMG_BUFFER[ix][iy];
        var row: u8[PIXEL_BMP_BYTES]: [4]u8 = {pixel, pixel, pixel, 0};
        try std.posix.write(std.posix.STDOUT_FILENO, row);
    }
}

pub fn main() !void {
    read_img();
    write_img();
}
