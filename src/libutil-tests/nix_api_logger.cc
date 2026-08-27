#include "nix_api_logger_internal.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nix {
namespace {

struct CapturedActivity
{
    std::string type;
    std::vector<int> fieldTypes;
    std::vector<std::string> stringValues;
};

struct CapturedEffect
{
    std::string kind;
    std::string subject;
    std::optional<std::string> detail;
};

void captureEffect(
    const char * kind,
    size_t kindLen,
    const char * subject,
    size_t subjectLen,
    const char * detail,
    size_t detailLen,
    void * userData)
{
    auto & captured = *static_cast<CapturedEffect *>(userData);
    captured.kind.assign(kind, kindLen);
    captured.subject.assign(subject, subjectLen);
    if (detail)
        captured.detail.emplace(detail, detailLen);
}

void captureActivity(
    uint64_t,
    const char *,
    const char * activityType,
    size_t fieldCount,
    const int * fieldTypes,
    const int64_t *,
    const char * const * stringValues,
    uint64_t,
    void * userData)
{
    auto & captured = *static_cast<CapturedActivity *>(userData);
    captured.type = activityType;
    for (size_t i = 0; i < fieldCount; ++i) {
        captured.fieldTypes.push_back(fieldTypes[i]);
        captured.stringValues.emplace_back(stringValues[i] ? stringValues[i] : "");
    }
}

TEST(CallbackLogger, ReportsEvalEffectWithoutActivity)
{
    CapturedActivity activity;
    CapturedEffect effect;
    CallbackLogger logger(captureActivity, nullptr, nullptr, nullptr, &activity);
    logger.setEvalEffectCallback(captureEffect, &effect);

    logger.evalEffect("evaluated-file", "/some/source/path/default.nix", "cached");

    EXPECT_TRUE(activity.type.empty());
    EXPECT_EQ(effect.kind, "evaluated-file");
    EXPECT_EQ(effect.subject, "/some/source/path/default.nix");
    EXPECT_EQ(effect.detail, "cached");
}

} // namespace
} // namespace nix
