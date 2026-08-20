#include "BankModel.h"

#include <algorithm>
#include <array>
#include <fstream>

namespace {

std::uint16_t readWordLE(const std::vector<std::uint8_t>& data, std::size_t pos) {
    if (pos + 1 >= data.size()) {
        return 0;
    }
    return static_cast<std::uint16_t>(data[pos]) |
           (static_cast<std::uint16_t>(data[pos + 1]) << 8);
}

void writeWordLE(std::vector<std::uint8_t>& data, std::size_t pos, std::uint16_t value) {
    if (pos + 1 >= data.size()) {
        return;
    }
    data[pos] = static_cast<std::uint8_t>(value & 0xFFu);
    data[pos + 1] = static_cast<std::uint8_t>((value >> 8u) & 0xFFu);
}

std::string stemUtf8(const std::filesystem::path& filePath) {
    return filePath.stem().string();
}

std::vector<std::uint8_t> readBinaryFile(const std::filesystem::path& filePath) {
    std::ifstream in(filePath, std::ios::binary);
    if (!in) {
        return {};
    }

    in.seekg(0, std::ios::end);
    const auto size = in.tellg();
    if (size <= 0) {
        return {};
    }
    in.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!in) {
        return {};
    }

    return data;
}

bool writeBinaryFile(const std::filesystem::path& filePath, const std::vector<std::uint8_t>& data) {
    std::ofstream out(filePath, std::ios::binary);
    if (!out) {
        return false;
    }
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(out);
}

}  // namespace

BankModel::BankModel() {
    reset();
}

void BankModel::reset() {
    effects_.clear();
    effects_.push_back(makeDefaultEffect(makeDefaultName(1)));
}

std::size_t BankModel::effectCount() const {
    return effects_.size();
}

const AyfxEffect& BankModel::effect(std::size_t index) const {
    return effects_.at(index);
}

AyfxEffect& BankModel::effect(std::size_t index) {
    return effects_.at(index);
}

std::size_t BankModel::effectRealLength(std::size_t index) const {
    const auto& frames = effects_.at(index).frames;
    for (std::size_t i = frames.size(); i > 0; --i) {
        if (frames[i - 1].volume > 0) {
            return i;
        }
    }
    return 0;
}

bool BankModel::loadEffect(std::size_t index, const std::filesystem::path& filePath) {
    if (index >= effects_.size()) {
        return false;
    }

    const auto data = readBinaryFile(filePath);
    if (data.empty()) {
        return false;
    }

    decodeEffect(index, data, 0, data.size());
    effects_[index].name = stemUtf8(filePath);
    return true;
}

bool BankModel::saveEffect(std::size_t index, const std::filesystem::path& filePath) const {
    if (index >= effects_.size()) {
        return false;
    }
    return writeBinaryFile(filePath, encodeEffect(index));
}

bool BankModel::loadBank(const std::filesystem::path& filePath) {
    const auto data = readBinaryFile(filePath);
    if (data.size() < 3) {
        return false;
    }

    reset();

    std::size_t count = data[0] == 0 ? kMaxEffects : static_cast<std::size_t>(data[0]);
    if (count == 0 || count > kMaxEffects) {
        return false;
    }

    if (data.size() < (1 + count * 2)) {
        return false;
    }

    effects_.assign(count, makeDefaultEffect(""));

    for (std::size_t i = 0; i < count; ++i) {
        const auto rel = readWordLE(data, 1 + i * 2);
        const std::size_t off = static_cast<std::size_t>(rel) + 2 + i * 2;
        if (off >= data.size()) {
            effects_[i] = makeDefaultEffect(makeDefaultName(i + 1));
            continue;
        }

        std::size_t len = 0;
        if (i + 1 < count) {
            const auto nextRel = readWordLE(data, 1 + (i + 1) * 2);
            const std::size_t nextOff = static_cast<std::size_t>(nextRel) + 2 + (i + 1) * 2;
            len = (nextOff > off) ? (nextOff - off) : 0;
        } else {
            len = data.size() - off;
        }

        const std::size_t decodedLen = decodeEffect(i, data, off, len);

        if (decodedLen < len && off + decodedLen < data.size()) {
            const char* rawName = reinterpret_cast<const char*>(data.data() + off + decodedLen);
            effects_[i].name = std::string(rawName);
        } else {
            effects_[i].name = makeDefaultName(i + 1);
        }
    }

    return true;
}

bool BankModel::saveBank(const std::filesystem::path& filePath, bool includeNames) const {
    if (effects_.empty() || effects_.size() > kMaxEffects) {
        return false;
    }

    std::vector<std::uint8_t> data;
    data.resize(1 + effects_.size() * 2, 0);
    data[0] = static_cast<std::uint8_t>(effects_.size() & 0xFFu);

    std::size_t writePos = data.size();

    for (std::size_t i = 0; i < effects_.size(); ++i) {
        const auto encoded = encodeEffect(i);

        const std::size_t rel = writePos - (2 + i * 2);
        writeWordLE(data, 1 + i * 2, static_cast<std::uint16_t>(rel & 0xFFFFu));

        data.insert(data.end(), encoded.begin(), encoded.end());
        writePos = data.size();

        if (includeNames && !effects_[i].name.empty()) {
            data.insert(data.end(), effects_[i].name.begin(), effects_[i].name.end());
            data.push_back(0);
            writePos = data.size();
        }
    }

    return writeBinaryFile(filePath, data);
}

bool BankModel::addEffect() {
    if (effects_.size() >= kMaxEffects) {
        return false;
    }
    effects_.push_back(makeDefaultEffect(makeDefaultName(effects_.size() + 1)));
    return true;
}

bool BankModel::deleteEffect(std::size_t index) {
    if (index >= effects_.size()) {
        return false;
    }

    if (effects_.size() == 1) {
        effects_[0] = makeDefaultEffect(makeDefaultName(1));
        return true;
    }

    effects_.erase(effects_.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool BankModel::insertEffect(std::size_t index) {
    if (index > effects_.size() || effects_.size() >= kMaxEffects) {
        return false;
    }

    std::array<char, 32> name{};
    std::snprintf(name.data(), name.size(), "inserted%03u", static_cast<unsigned>(index));
    effects_.insert(effects_.begin() + static_cast<std::ptrdiff_t>(index), makeDefaultEffect(name.data()));
    return true;
}

AyfxEffect BankModel::makeDefaultEffect(const std::string& name) {
    AyfxEffect effect;
    effect.name = name;
    effect.frames.resize(kMaxFrames);
    return effect;
}

std::string BankModel::makeDefaultName(std::size_t oneBasedIndex) {
    std::array<char, 32> name{};
    std::snprintf(name.data(), name.size(), "noname%03u", static_cast<unsigned>(oneBasedIndex));
    return name.data();
}

std::size_t BankModel::decodeEffect(std::size_t index, const std::vector<std::uint8_t>& data, std::size_t offset, std::size_t size) {
    auto& outFrames = effects_.at(index).frames;
    std::fill(outFrames.begin(), outFrames.end(), AyfxCell{});

    std::size_t pp = offset;
    const std::size_t end = std::min(offset + size, data.size());
    std::size_t pd = 0;
    std::uint16_t tone = 0;
    std::uint8_t noise = 0;

    while (pp < end && pd < outFrames.size()) {
        const std::uint8_t it = data[pp++];

        if (it & (1u << 5u)) {
            if (pp + 1 >= end) {
                break;
            }
            tone = static_cast<std::uint16_t>(readWordLE(data, pp) & 0x0FFFu);
            pp += 2;
        }

        if (it & (1u << 6u)) {
            if (pp >= end) {
                break;
            }
            noise = data[pp++];
            if (it == 0xD0u && noise >= 0x20u) {
                break;
            }
            noise &= 0x1Fu;
        }

        AyfxCell cell;
        cell.tone = tone;
        cell.noise = noise;
        cell.volume = static_cast<std::uint8_t>(it & 0x0Fu);
        cell.toneEnable = (it & (1u << 4u)) == 0;
        cell.noiseEnable = (it & (1u << 7u)) == 0;
        outFrames[pd++] = cell;
    }

    return pp - offset;
}

std::vector<std::uint8_t> BankModel::encodeEffect(std::size_t index) const {
    const auto& frames = effects_.at(index).frames;
    const std::size_t fxLen = effectRealLength(index);

    std::vector<std::uint8_t> out;
    out.reserve(fxLen * 4 + 2);

    std::uint16_t tone = 0xFFFFu;
    std::uint8_t noise = 0xFFu;

    for (std::size_t i = 0; i < fxLen; ++i) {
        const auto& cell = frames[i];

        std::uint8_t it = static_cast<std::uint8_t>(cell.volume & 0x0Fu);
        it |= cell.toneEnable ? 0u : static_cast<std::uint8_t>(1u << 4u);
        it |= cell.noiseEnable ? 0u : static_cast<std::uint8_t>(1u << 7u);

        if (cell.tone != tone) {
            tone = cell.tone;
            it |= static_cast<std::uint8_t>(1u << 5u);
        }

        if (cell.noise != noise) {
            noise = cell.noise;
            it |= static_cast<std::uint8_t>(1u << 6u);
        }

        out.push_back(it);

        if (it & (1u << 5u)) {
            out.push_back(static_cast<std::uint8_t>(tone & 0xFFu));
            out.push_back(static_cast<std::uint8_t>((tone >> 8u) & 0x0Fu));
        }

        if (it & (1u << 6u)) {
            out.push_back(static_cast<std::uint8_t>(noise & 0x1Fu));
        }
    }

    out.push_back(0xD0u);
    out.push_back(0x20u);
    return out;
}
