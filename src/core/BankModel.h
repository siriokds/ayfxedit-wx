#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct AyfxCell {
    std::uint16_t tone = 0;
    std::uint8_t noise = 0;
    std::uint8_t volume = 0;
    bool toneEnable = false;
    bool noiseEnable = false;
    bool selected = false;
};

struct AyfxEffect {
    std::string name;
    std::vector<AyfxCell> frames;
};

class BankModel {
public:
    static constexpr std::size_t kMaxEffects = 256;
    static constexpr std::size_t kMaxFrames = 4096;

    BankModel();

    void reset();
    [[nodiscard]] std::size_t effectCount() const;
    [[nodiscard]] const AyfxEffect& effect(std::size_t index) const;
    [[nodiscard]] AyfxEffect& effect(std::size_t index);

    [[nodiscard]] std::size_t effectRealLength(std::size_t index) const;

    bool loadEffect(std::size_t index, const std::filesystem::path& filePath);
    bool saveEffect(std::size_t index, const std::filesystem::path& filePath) const;

    bool loadBank(const std::filesystem::path& filePath);
    bool saveBank(const std::filesystem::path& filePath, bool includeNames) const;

    bool addEffect();
    bool deleteEffect(std::size_t index);
    bool insertEffect(std::size_t index);

private:
    static AyfxEffect makeDefaultEffect(const std::string& name);
    static std::string makeDefaultName(std::size_t oneBasedIndex);

    std::size_t decodeEffect(std::size_t index, const std::vector<std::uint8_t>& data, std::size_t offset, std::size_t size);
    [[nodiscard]] std::vector<std::uint8_t> encodeEffect(std::size_t index) const;

    std::vector<AyfxEffect> effects_;
};
