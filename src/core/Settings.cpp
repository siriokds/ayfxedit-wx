#include "Settings.h"

#include <wx/filename.h>
#include <wx/stdpaths.h>

#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <system_error>

namespace {

std::string JsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += c; break;
        }
    }
    return out;
}

std::string ToJson(const Settings& s) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"version\": 1,\n";
    out << "  \"general\": {\n";
    out << "    \"singleInstance\": " << (s.singleInstance ? "true" : "false") << ",\n";
    out << "    \"confirmDeleteEffect\": " << (s.confirmDeleteEffect ? "true" : "false") << "\n";
    out << "  },\n";
    out << "  \"appearance\": {\n";
    out << "    \"uiFontFamily\": \"" << JsonEscape(s.uiFontFamily) << "\",\n";
    out << "    \"uiFontSize\": " << s.uiFontSize << ",\n";
    out << "    \"monoFontFamily\": \"" << JsonEscape(s.monoFontFamily) << "\",\n";
    out << "    \"monoFontSize\": " << s.monoFontSize << ",\n";
    out << "    \"rowPaddingPx\": " << s.rowPaddingPx << "\n";
    out << "  },\n";
    out << "  \"audio\": {\n";
    out << "    \"chipType\": \"" << JsonEscape(s.chipType) << "\",\n";
    out << "    \"clockHz\": " << s.clockHz << ",\n";
    out << "    \"lowpassHz\": " << s.lowpassHz << ",\n";
    out << "    \"sampleRate\": " << s.sampleRate << ",\n";
    out << "    \"volume\": " << s.volume << ",\n";
    out << "    \"outputDevice\": \"" << JsonEscape(s.outputDevice) << "\"\n";
    out << "  },\n";
    out << "  \"window\": {\n";
    out << "    \"x\": " << s.windowGeometry.x << ",\n";
    out << "    \"y\": " << s.windowGeometry.y << ",\n";
    out << "    \"w\": " << s.windowGeometry.w << ",\n";
    out << "    \"h\": " << s.windowGeometry.h << ",\n";
    out << "    \"maximized\": " << (s.windowGeometry.maximized ? "true" : "false") << ",\n";
    out << "    \"posValid\": " << (s.windowGeometry.posValid ? "true" : "false") << "\n";
    out << "  }\n";
    out << "}\n";
    return out.str();
}

// Minimal recursive-descent JSON reader -- just enough of the grammar
// (objects, strings, numbers, booleans, null) to read back what ToJson()
// above writes. Not a general-purpose/spec-complete parser (no unicode
// escapes, no arrays -- our schema doesn't use them).
class JsonParser {
public:
    struct Value {
        enum class Type { Null, Bool, Number, String, Object } type = Type::Null;
        bool boolValue = false;
        double numberValue = 0;
        std::string stringValue;
        std::map<std::string, Value> object;

        const Value* find(const std::string& key) const {
            auto it = object.find(key);
            return it != object.end() ? &it->second : nullptr;
        }
        std::string asString(const std::string& def) const {
            return type == Type::String ? stringValue : def;
        }
        int asInt(int def) const {
            return type == Type::Number ? static_cast<int>(numberValue) : def;
        }
        bool asBool(bool def) const {
            return type == Type::Bool ? boolValue : def;
        }
    };

    explicit JsonParser(const std::string& text) : text_(text) {}

    bool parse(Value& out) {
        skipWs();
        return parseValue(out);
    }

private:
    const std::string& text_;
    std::size_t pos_ = 0;

    void skipWs() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) ++pos_;
    }
    char peek() const { return pos_ < text_.size() ? text_[pos_] : '\0'; }
    char get() { return pos_ < text_.size() ? text_[pos_++] : '\0'; }

    bool parseValue(Value& out) {
        skipWs();
        const char c = peek();
        if (c == '{') return parseObject(out);
        if (c == '"') return parseString(out);
        if (c == 't' || c == 'f') return parseBool(out);
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parseNumber(out);
        if (c == 'n') { pos_ += 4; out.type = Value::Type::Null; return true; }
        return false;
    }

    bool parseObject(Value& out) {
        out.type = Value::Type::Object;
        get();  // '{'
        skipWs();
        if (peek() == '}') { get(); return true; }
        while (true) {
            skipWs();
            Value key;
            if (!parseString(key)) return false;
            skipWs();
            if (get() != ':') return false;
            Value v;
            if (!parseValue(v)) return false;
            out.object[key.stringValue] = v;
            skipWs();
            const char c = get();
            if (c == ',') continue;
            if (c == '}') break;
            return false;
        }
        return true;
    }

    bool parseString(Value& out) {
        if (get() != '"') return false;
        std::string s;
        while (true) {
            if (pos_ >= text_.size()) return false;
            const char c = get();
            if (c == '"') break;
            if (c == '\\') {
                const char e = get();
                switch (e) {
                case '"': s += '"'; break;
                case '\\': s += '\\'; break;
                case 'n': s += '\n'; break;
                case 'r': s += '\r'; break;
                case 't': s += '\t'; break;
                default: s += e; break;
                }
            } else {
                s += c;
            }
        }
        out.type = Value::Type::String;
        out.stringValue = s;
        return true;
    }

    bool parseBool(Value& out) {
        if (text_.compare(pos_, 4, "true") == 0) {
            pos_ += 4;
            out.type = Value::Type::Bool;
            out.boolValue = true;
            return true;
        }
        if (text_.compare(pos_, 5, "false") == 0) {
            pos_ += 5;
            out.type = Value::Type::Bool;
            out.boolValue = false;
            return true;
        }
        return false;
    }

    bool parseNumber(Value& out) {
        const std::size_t start = pos_;
        if (peek() == '-') get();
        while (std::isdigit(static_cast<unsigned char>(peek()))) get();
        if (peek() == '.') {
            get();
            while (std::isdigit(static_cast<unsigned char>(peek()))) get();
        }
        if (pos_ == start) return false;
        out.type = Value::Type::Number;
        out.numberValue = std::stod(text_.substr(start, pos_ - start));
        return true;
    }
};

}  // namespace

std::filesystem::path SettingsFilePath() {
    const wxString dir = wxStandardPaths::Get().GetUserDataDir();
    wxFileName::Mkdir(dir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    return std::filesystem::path(dir.ToStdString()) / "settings.json";
}

Settings LoadSettings() {
    Settings s;

    std::ifstream in(SettingsFilePath(), std::ios::binary);
    if (!in) {
        return s;  // no file yet -> defaults
    }
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

    JsonParser parser(text);
    JsonParser::Value root;
    if (!parser.parse(root) || root.type != JsonParser::Value::Type::Object) {
        return s;  // corrupt/unreadable -> defaults
    }

    if (const auto* general = root.find("general")) {
        s.singleInstance = general->find("singleInstance")
                                ? general->find("singleInstance")->asBool(s.singleInstance)
                                : s.singleInstance;
        s.confirmDeleteEffect = general->find("confirmDeleteEffect")
                                     ? general->find("confirmDeleteEffect")->asBool(s.confirmDeleteEffect)
                                     : s.confirmDeleteEffect;
    }

    if (const auto* appearance = root.find("appearance")) {
        if (const auto* v = appearance->find("uiFontFamily")) s.uiFontFamily = v->asString(s.uiFontFamily);
        if (const auto* v = appearance->find("uiFontSize")) s.uiFontSize = v->asInt(s.uiFontSize);
        if (const auto* v = appearance->find("monoFontFamily")) s.monoFontFamily = v->asString(s.monoFontFamily);
        if (const auto* v = appearance->find("monoFontSize")) s.monoFontSize = v->asInt(s.monoFontSize);
        if (const auto* v = appearance->find("rowPaddingPx")) s.rowPaddingPx = v->asInt(s.rowPaddingPx);
    }

    if (const auto* audio = root.find("audio")) {
        if (const auto* v = audio->find("chipType")) s.chipType = v->asString(s.chipType);
        if (const auto* v = audio->find("clockHz")) s.clockHz = v->asInt(s.clockHz);
        if (const auto* v = audio->find("lowpassHz")) s.lowpassHz = v->asInt(s.lowpassHz);
        if (const auto* v = audio->find("sampleRate")) s.sampleRate = v->asInt(s.sampleRate);
        if (const auto* v = audio->find("volume")) s.volume = v->asInt(s.volume);
        if (const auto* v = audio->find("outputDevice")) s.outputDevice = v->asString(s.outputDevice);
    }

    if (const auto* window = root.find("window")) {
        if (const auto* v = window->find("x")) s.windowGeometry.x = v->asInt(s.windowGeometry.x);
        if (const auto* v = window->find("y")) s.windowGeometry.y = v->asInt(s.windowGeometry.y);
        if (const auto* v = window->find("w")) s.windowGeometry.w = v->asInt(s.windowGeometry.w);
        if (const auto* v = window->find("h")) s.windowGeometry.h = v->asInt(s.windowGeometry.h);
        if (const auto* v = window->find("maximized")) s.windowGeometry.maximized = v->asBool(s.windowGeometry.maximized);
        if (const auto* v = window->find("posValid")) s.windowGeometry.posValid = v->asBool(s.windowGeometry.posValid);
    }

    // A sampleRate of 0 (or negative) would leave the resampler with no
    // target rate and produce silent output -- guard against ever loading
    // one, regardless of how it ended up on disk.
    if (s.sampleRate <= 0) {
        s.sampleRate = Settings().sampleRate;
    }

    return s;
}

bool SaveSettings(const Settings& settings) {
    const auto path = SettingsFilePath();
    const auto tmpPath = path.string() + ".tmp";

    {
        std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << ToJson(settings);
        if (!out) return false;
    }

    std::error_code ec;
    std::filesystem::rename(tmpPath, path, ec);
    if (ec) {
        // rename() can fail across filesystems/devices; fall back to copy+remove.
        std::filesystem::copy_file(tmpPath, path, std::filesystem::copy_options::overwrite_existing, ec);
        std::filesystem::remove(tmpPath, ec);
        return !ec;
    }
    return true;
}
