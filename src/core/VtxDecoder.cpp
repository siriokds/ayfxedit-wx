#include "VtxDecoder.h"

#include "../../third_party/lh5/ar.h"

#include <cstring>
#include <exception>

// Globals expected by the vendored lh5 decoder (see third_party/lh5/ar.h),
// matching how the original ayfxedit's import_vtx.h wired the same sources.
int unpackable = 0;
ulong compsize = 0, origsize = 0;

namespace {
const std::uint8_t* g_src = nullptr;
uchar g_decodeBuf[DICSIZ + 1000];
}  // namespace

uchar getc_arcfile() {
    return *g_src++;
}

std::vector<std::uint8_t> DecodeLh5(const std::uint8_t* src, std::size_t srcSize, std::size_t dstSize) {
    std::vector<std::uint8_t> out(dstSize);
    if (dstSize == 0) {
        return out;
    }

    g_src = src;
    compsize = static_cast<ulong>(srcSize);
    origsize = static_cast<ulong>(dstSize);

    try {
        decode_start();
        std::size_t remaining = dstSize;
        std::uint8_t* dst = out.data();
        while (remaining > 0) {
            const unsigned blockSize = remaining < DICSIZ ? static_cast<unsigned>(remaining) : DICSIZ;
            decode(blockSize, g_decodeBuf);
            std::memcpy(dst, g_decodeBuf, blockSize);
            dst += blockSize;
            remaining -= blockSize;
        }
    } catch (const std::exception&) {
        return {};  // malformed/corrupt compressed data
    }

    return out;
}
