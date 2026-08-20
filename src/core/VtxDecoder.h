#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Decompresses an LH5 ("-lh5-", LZSS+Huffman) buffer, as used by Vortex
// Tracker's VTX file format. Wraps the vendored decoder at third_party/lh5/.
// Returns the decompressed bytes, or an empty vector if decompression fails
// (malformed/truncated input).
std::vector<std::uint8_t> DecodeLh5(const std::uint8_t* src, std::size_t srcSize, std::size_t dstSize);
