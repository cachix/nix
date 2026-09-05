#include "nix/store/derivation/cbor.hh"
#include "nix/util/json-utils.hh"

#include <nlohmann/json.hpp>
#include <algorithm>

namespace nix::derivation {
namespace {

using nlohmann::json;

[[noreturn]] void invalid(std::string_view message)
{
    throw FormatError("invalid derivation CBOR: %s", message);
}

void checkText(const std::string & text)
{
    (void) json(text).dump();
}

class Reader
{
    std::string_view bytes;
    size_t position = 0;

    uint8_t byte()
    {
        if (position == bytes.size())
            invalid("unexpected end of input");
        return static_cast<uint8_t>(bytes[position++]);
    }

    uint64_t length(uint8_t info)
    {
        if (info < 24)
            return info;
        if (info > 27)
            invalid("reserved or indefinite length");
        uint64_t value = 0;
        for (unsigned i = 0; i < (1u << (info - 24)); ++i)
            value = (value << 8) | byte();
        return value;
    }

public:
    explicit Reader(std::string_view bytes)
        : bytes(bytes)
    {
    }

    json read(unsigned depth = 0)
    {
        if (depth > 520)
            invalid("nesting limit exceeded");
        auto initial = byte();
        auto major = initial >> 5;
        if (initial == 0xf4 || initial == 0xf5)
            return initial == 0xf5;
        if (major != 0 && major != 2 && major != 3 && major != 4 && major != 5)
            invalid("unsupported CBOR type");
        auto size = length(initial & 31);
        if (major == 0)
            return size;
        if (size > (bytes.size() - position) / (major == 5 ? 2 : 1))
            invalid("length exceeds remaining input");
        if (major == 2 || major == 3) {
            auto value = bytes.substr(position, size);
            position += size;
            if (major == 2)
                return json::binary(std::vector<uint8_t>(value.begin(), value.end()));
            std::string text(value);
            checkText(text);
            return text;
        }
        auto value = major == 4 ? json::array() : json::object();
        for (uint64_t i = 0; i < size; ++i) {
            if (major == 4)
                value.push_back(read(depth + 1));
            else {
                auto key = read(depth + 1);
                if (!key.is_string())
                    invalid("map key must be text");
                auto name = key.get<std::string>();
                if (value.contains(name))
                    invalid("duplicate map key");
                value[name] = read(depth + 1);
            }
        }
        return value;
    }

    bool done() const
    {
        return position == bytes.size();
    }
};

void fields(
    const json & value,
    std::initializer_list<std::string_view> required,
    std::initializer_list<std::string_view> optional = {})
{
    if (!value.is_object())
        invalid("expected map");
    for (auto key : required)
        if (!value.contains(key))
            invalid("missing field");
    for (auto & [key, ignored] : value.items())
        if (std::find(required.begin(), required.end(), key) == required.end()
            && std::find(optional.begin(), optional.end(), key) == optional.end())
            invalid("unknown field");
}

void stringSet(const json & value)
{
    if (!value.is_array())
        invalid("expected set array");
    std::set<std::string> seen;
    for (auto & entry : value) {
        if (!entry.is_string())
            invalid("set entry must be text");
        if (!seen.insert(entry.get<std::string>()).second)
            invalid("duplicate set entry");
    }
}

void input(const json & value, unsigned depth = 0)
{
    if (depth > 256)
        invalid("dynamic input nesting exceeds 256 levels");
    fields(value, {"outputs", "dynamicOutputs"});
    stringSet(value.at("outputs"));
    for (auto & [name, child] : getObject(value.at("dynamicOutputs")))
        input(child, depth + 1);
}

std::string binaryString(const json & value)
{
    if (!value.is_binary())
        invalid("expected byte string");
    auto & bytes = value.get_binary();
    return std::string(bytes.begin(), bytes.end());
}

std::optional<StructuredAttrs> prepareJson(json & value)
{
    fields(value, {"version", "name", "system", "builder", "args", "env", "outputs", "inputs"}, {"structuredAttrs"});
    if (!value.at("version").is_number_unsigned() || value.at("version") != expectedCborVersion)
        invalid("unsupported version");
    fields(value.at("inputs"), {"srcs", "drvs"});
    stringSet(value.at("inputs").at("srcs"));
    for (auto & [path, node] : getObject(value.at("inputs").at("drvs")))
        input(node);
    for (auto & [name, output] : getObject(value.at("outputs"))) {
        if (output.contains("impure") && output.at("impure") != json(true))
            invalid("impure must be true");
    }
    if (!value.at("env").is_object())
        invalid("env must be a map");
    for (auto & entry : value.at("env"))
        entry = binaryString(entry);
    std::optional<StructuredAttrs> attrs;
    if (value.contains("structuredAttrs")) {
        attrs = StructuredAttrs::parse(binaryString(value.at("structuredAttrs")));
        value["structuredAttrs"] = attrs->structuredAttrs;
    }
    value["version"] = expectedJsonVersionDerivation;
    return attrs;
}

void writeHead(std::string & bytes, uint8_t major, uint64_t size)
{
    if (size < 24) {
        bytes += static_cast<char>((major << 5) | size);
        return;
    }
    unsigned width = size <= 0xff ? 1 : size <= 0xffff ? 2 : size <= 0xffffffff ? 4 : 8;
    bytes += static_cast<char>((major << 5) | (width == 1 ? 24 : width == 2 ? 25 : width == 4 ? 26 : 27));
    for (unsigned i = width; i; --i)
        bytes += static_cast<char>(size >> ((i - 1) * 8));
}

void writeValue(std::string & bytes, const json & value)
{
    if (value.is_object()) {
        std::vector<std::string> keys;
        for (auto & [key, ignored] : value.items())
            keys.push_back(key);
        std::sort(keys.begin(), keys.end(), [](auto & a, auto & b) {
            return a.size() != b.size() ? a.size() < b.size() : a < b;
        });
        writeHead(bytes, 5, keys.size());
        for (auto & key : keys) {
            writeValue(bytes, key);
            writeValue(bytes, value.at(key));
        }
    } else if (value.is_array()) {
        writeHead(bytes, 4, value.size());
        for (auto & entry : value)
            writeValue(bytes, entry);
    } else if (value.is_string()) {
        auto & text = value.get_ref<const std::string &>();
        checkText(text);
        writeHead(bytes, 3, text.size());
        bytes += text;
    } else if (value.is_binary()) {
        auto & data = value.get_binary();
        writeHead(bytes, 2, data.size());
        bytes.append(data.begin(), data.end());
    } else if (value.is_boolean())
        bytes += value.get<bool>() ? '\xf5' : '\xf4';
    else if (value.is_number_unsigned())
        writeHead(bytes, 0, value.get<uint64_t>());
    else
        invalid("unsupported value");
}

void checkOutputs(const Full & drv)
{
    for (auto & [name, output] : drv.outputs) {
        if (auto fixed = std::get_if<Output::CAFixed>(&output.raw)) {
            auto algo = fixed->ca.hash.algo;
            if (fixed->ca.method == ContentAddressMethod::Raw::Text && algo != HashAlgorithm::SHA256)
                invalid("text content addressing requires SHA-256");
            if (fixed->ca.method == ContentAddressMethod::Raw::Git && algo != HashAlgorithm::SHA1
                && algo != HashAlgorithm::SHA256)
                invalid("Git content addressing requires SHA-1 or SHA-256");
        }
    }
}

json toCborValue(const Full & drv)
{
    try {
        checkOutputs(drv);
        json value = drv;
        value["version"] = expectedCborVersion;
        for (auto & entry : value.at("env")) {
            auto & text = entry.get_ref<const std::string &>();
            entry = json::binary(std::vector<uint8_t>(text.begin(), text.end()));
        }
        if (drv.structuredAttrs) {
            auto text = drv.structuredAttrs->unparse().second;
            value["structuredAttrs"] = json::binary(std::vector<uint8_t>(text.begin(), text.end()));
        }
        auto & sources = value.at("inputs").at("srcs");
        std::sort(sources.begin(), sources.end());
        for (auto & [path, node] : getObject(value.at("inputs").at("drvs")))
            input(node);
        return value;
    } catch (json::exception & e) {
        invalid(e.what());
    }
}

} // namespace

std::string toCbor(const Full & drv)
{
    try {
        std::string bytes;
        writeValue(bytes, toCborValue(drv));
        return bytes;
    } catch (json::exception & e) {
        invalid(e.what());
    }
}

std::string toCbor(const std::map<StorePath, Full> & drvs)
{
    try {
        auto entries = json::object();
        for (auto & [path, drv] : drvs)
            entries[path.to_string()] = toCborValue(drv);
        std::string bytes;
        writeValue(bytes, json{{"version", expectedCborVersion}, {"derivations", std::move(entries)}});
        return bytes;
    } catch (json::exception & e) {
        invalid(e.what());
    }
}

Full parseCbor(std::string_view bytes, const ExperimentalFeatureSettings & xpSettings)
{
    try {
        Reader reader(bytes);
        auto value = reader.read();
        if (!reader.done())
            invalid("trailing data");
        auto attrs = prepareJson(value);
        auto drv = nlohmann::adl_serializer<Full>::from_json(value, xpSettings);
        drv.structuredAttrs = std::move(attrs);
        checkOutputs(drv);
        return drv;
    } catch (json::exception & e) {
        invalid(e.what());
    }
}

} // namespace nix::derivation
