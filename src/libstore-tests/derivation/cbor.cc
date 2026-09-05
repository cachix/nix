#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "nix/store/derivation/cbor.hh"
#include "nix/store/derivation/aterm.hh"
#include "derivation/test-support.hh"

namespace nix {
namespace {

using nlohmann::json;
using namespace std::string_literals;

const auto golden =
    "\xa8\x63"
    "env\xa2\x61z\x41\x00\x62"
    "aa\x41\xff\x64"
    "args\x81\x61"
    "a\x64"
    "name\x61n\x66inputs\xa2\x64"
    "drvs\xa0\x64srcs\x80\x66system\x61s\x67"
    "builder\x61"
    "b\x67outputs\xa0\x67version\x01"s;

std::string encode(const json & value)
{
    auto bytes = json::to_cbor(value);
    return {bytes.begin(), bytes.end()};
}

TEST_F(DerivationTest, CborReferenceGolden)
{
    auto drv = derivation::parseCbor(golden, mockXpSettings);
    EXPECT_EQ(drv.name, "n");
    EXPECT_EQ(drv.env.at("aa"), "\xff"s);
    EXPECT_EQ(drv.env.at("z"), "\x00"s);
    EXPECT_EQ(derivation::toCbor(drv), golden);
    EXPECT_EQ(
        derivation::unparse(drv, *store), "Derive([],[],[],\"s\",\"b\",[\"a\"],[(\"aa\",\"\xff\"),(\"z\",\"\x00\")])"s);
}

TEST_F(DerivationTest, CborRejectsTruncationsAndInvalidWireTypes)
{
    for (size_t size = 0; size < golden.size(); ++size)
        EXPECT_THROW(derivation::parseCbor(std::string_view(golden).substr(0, size)), Error) << size;
    for (auto bytes :
         {golden + "\x00"s,
          "\xc0" + golden,
          "\xbf\xff"s,
          "\xa2\x61x\x00\x61x\x00"s,
          "\xbb\xff\xff\xff\xff\xff\xff\xff\xff"s})
        EXPECT_THROW(derivation::parseCbor(bytes), Error);
}

TEST_F(DerivationTest, CborRejectsInvalidSchema)
{
    auto original = json::from_cbor(golden);
    for (auto & [key, ignored] : original.items()) {
        auto value = original;
        value.erase(key);
        EXPECT_THROW(derivation::parseCbor(encode(value)), Error) << key;
    }
    for (auto bad : {json(nullptr), json("text"), json::array({1, 2})}) {
        auto value = original;
        value["env"]["z"] = bad;
        EXPECT_THROW(derivation::parseCbor(encode(value)), Error);
    }
    auto value = original;
    value["extra"] = 1;
    EXPECT_THROW(derivation::parseCbor(encode(value)), Error);
    value = original;
    value["version"] = 4;
    EXPECT_THROW(derivation::parseCbor(encode(value)), Error);
    value = original;
    value["inputs"]["srcs"] = {"c015dhfh5l0lp6wxyvdn7bmwhbbr6hr9-dep", "c015dhfh5l0lp6wxyvdn7bmwhbbr6hr9-dep"};
    EXPECT_THROW(derivation::parseCbor(encode(value)), Error);
    value = original;
    value["name"] = "\xff";
    EXPECT_THROW(derivation::parseCbor(encode(value)), Error);
}

TEST_F(DerivationTest, CborNormalizesEncoding)
{
    auto value = json::from_cbor(golden);
    EXPECT_EQ(derivation::toCbor(derivation::parseCbor(encode(value))), golden);
    auto nonminimal = golden.substr(0, golden.size() - 1) + "\x18\x01";
    EXPECT_EQ(derivation::toCbor(derivation::parseCbor(nonminimal)), golden);
}

TEST_F(DerivationTest, CborDerivationCollection)
{
    auto drv = derivation::parseCbor(golden);
    StorePath first{"c015dhfh5l0lp6wxyvdn7bmwhbbr6hr9-first.drv"};
    StorePath second{"c015dhfh5l0lp6wxyvdn7bmwhbbr6hr9-second.drv"};
    std::map<StorePath, Derivation> drvs{{first, drv}, {second, drv}};
    auto bytes = derivation::toCbor(drvs);
    auto value = json::from_cbor(bytes);
    EXPECT_EQ(value.at("version"), 1);
    EXPECT_EQ(value.at("derivations").size(), 2u);
    for (auto & [path, entry] : value.at("derivations").items())
        EXPECT_EQ(derivation::toCbor(derivation::parseCbor(encode(entry))), golden);
    EXPECT_EQ(derivation::toCbor(drvs), bytes);
}

TEST_F(DynDerivationTest, CborFixtureRoundTrips)
{
    for (auto name : {"simple-derivation", "dyn-dep-derivation"}) {
        auto drv = nlohmann::adl_serializer<Derivation>::from_json(
            json::parse(readFile(goldenMaster(std::string(name) + ".json"))), mockXpSettings);
        auto decoded = derivation::parseCbor(derivation::toCbor(drv), mockXpSettings);
        EXPECT_EQ(decoded, drv);
        EXPECT_EQ(derivation::unparse(decoded, *store), derivation::unparse(drv, *store));
    }
}

TEST_F(DerivationTest, CborStructuredAttrs)
{
    auto drv = derivation::parseCbor(golden);
    drv.structuredAttrs = StructuredAttrs{json::parse(R"({"z":-0.0,"a":18446744073709551615})")};
    auto bytes = derivation::toCbor(drv);
    auto value = json::from_cbor(bytes);
    EXPECT_TRUE(value.at("structuredAttrs").is_binary());
    auto decoded = derivation::parseCbor(bytes);
    EXPECT_EQ(decoded, drv);
    EXPECT_EQ(derivation::unparse(decoded, *store), derivation::unparse(drv, *store));
    value["structuredAttrs"] = json::binary({'[', ']'});
    EXPECT_THROW(derivation::parseCbor(encode(value)), Error);
}

TEST_F(DerivationTest, CborStringLengthsAndUtf8)
{
    auto drv = derivation::parseCbor(golden);
    for (auto size : {0, 23, 24, 255, 256, 65535, 65536}) {
        drv.env["payload"] = std::string(size, '\xff');
        drv.builder = std::string(size, 'x');
        auto bytes = derivation::toCbor(drv);
        EXPECT_EQ(derivation::parseCbor(bytes), drv);
        EXPECT_EQ(json::from_cbor(bytes).at("builder"), drv.builder);
    }
    drv.name = "\xc3\xa9";
    EXPECT_EQ(derivation::parseCbor(derivation::toCbor(drv)), drv);
    drv.name = "\xff";
    EXPECT_THROW(derivation::toCbor(drv), Error);
}

TEST_F(DerivationTest, CborPreservesStructuredAttrsIdentity)
{
    for (const std::string text :
         {R"({ "z": 0, "a": 1 })",
          R"({"escaped":"\u0061","number":1e+02,"negativeZero":-0.0})",
          "{\n  \"a\": 1, \"a\": 2\n}\n"}) {
        auto drv = derivation::parseCbor(golden);
        drv.env.clear();
        drv.structuredAttrs = StructuredAttrs::parse(text);
        auto aterm = derivation::unparse(drv, *store);
        auto imported = derivation::parse(
            *store, std::string(aterm), drv.name, derivation::defaultSupportWindowsStoreDir, mockXpSettings);
        auto bytes = derivation::toCbor(imported);
        auto payload = json::from_cbor(bytes).at("structuredAttrs").get_binary();
        EXPECT_EQ(std::string(payload.begin(), payload.end()), text);
        auto decoded = derivation::parseCbor(bytes);
        EXPECT_EQ(decoded, imported);
        EXPECT_EQ(decoded.structuredAttrs->unparse().second, text);
        EXPECT_EQ(derivation::unparse(decoded, *store), aterm);
        EXPECT_EQ(computeStorePath(*store, decoded), computeStorePath(*store, imported));
        EXPECT_EQ(derivation::toCbor(decoded), bytes);

        auto collection = json::from_cbor(
            derivation::toCbor(std::map<StorePath, Derivation>{{computeStorePath(*store, imported), imported}}));
        EXPECT_EQ(collection.at("derivations").begin()->at("structuredAttrs"), json::binary(payload));
    }
}

TEST_F(DerivationTest, CborOutputVariants)
{
    mockXpSettings.set("experimental-features", "ca-derivations dynamic-derivations impure-derivations");
    for (auto name :
         {"inputAddressed", "caFixedFlat", "caFixedNAR", "caFixedText", "caFloating", "deferred", "impure"}) {
        auto value = json::from_cbor(golden);
        value["outputs"]["out"] = json::parse(readFile(goldenMaster("output-"s + name + ".json")));
        auto drv = derivation::parseCbor(encode(value), mockXpSettings);
        EXPECT_EQ(derivation::parseCbor(derivation::toCbor(drv), mockXpSettings), drv);
    }
}

TEST_F(DynDerivationTest, CborDynamicDepthLimit)
{
    auto value = json::from_cbor(golden);
    json node = {{"outputs", {"out"}}, {"dynamicOutputs", json::object()}};
    for (unsigned depth = 0; depth < 256; ++depth)
        node = {{"outputs", json::array()}, {"dynamicOutputs", {{"out", std::move(node)}}}};
    auto path = "c015dhfh5l0lp6wxyvdn7bmwhbbr6hr9-dep.drv";
    value["inputs"]["drvs"][path] = node;
    EXPECT_NO_THROW(derivation::parseCbor(encode(value), mockXpSettings));
    value["inputs"]["drvs"][path] = {{"outputs", json::array()}, {"dynamicOutputs", {{"out", node}}}};
    EXPECT_THROW(derivation::parseCbor(encode(value), mockXpSettings), Error);
}

} // namespace
} // namespace nix
