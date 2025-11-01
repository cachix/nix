#include "nix_api_logger_internal.h"
#include "nix_api_util.h"
#include "nix_api_util_internal.h"

namespace nix {

const char * CallbackLogger::activityTypeToString(ActivityType type)
{
    switch (type) {
    case actUnknown:
        return "unknown";
    case actCopyPath:
        return "copy-path";
    case actFileTransfer:
        return "file-transfer";
    case actRealise:
        return "realise";
    case actCopyPaths:
        return "copy-paths";
    case actBuilds:
        return "builds";
    case actBuild:
        return "build";
    case actOptimiseStore:
        return "optimise-store";
    case actVerifyPaths:
        return "verify-paths";
    case actSubstitute:
        return "substitute";
    case actQueryPathInfo:
        return "query-path-info";
    case actPostBuildHook:
        return "post-build-hook";
    case actBuildWaiting:
        return "build-waiting";
    case actFetchTree:
        return "fetch-tree";
    default:
        return "unknown";
    }
}

const char * CallbackLogger::resultTypeToString(ResultType type)
{
    switch (type) {
    case resFileLinked:
        return "file-linked";
    case resBuildLogLine:
        return "build-log-line";
    case resUntrustedPath:
        return "untrusted-path";
    case resCorruptedPath:
        return "corrupted-path";
    case resSetPhase:
        return "set-phase";
    case resProgress:
        return "progress";
    case resSetExpected:
        return "set-expected";
    case resPostBuildLogLine:
        return "post-build-log-line";
    case resFetchStatus:
        return "fetch-status";
    default:
        return "unknown";
    }
}

CallbackLogger::CallbackLogger(
    nix_activity_start_cb on_start,
    nix_activity_stop_cb on_stop,
    nix_activity_result_cb on_result,
    void * user_data)
    : on_start(on_start)
    , on_stop(on_stop)
    , on_result(on_result)
    , user_data(user_data)
{
}

void CallbackLogger::startActivity(
    ActivityId act,
    Verbosity lvl,
    ActivityType type,
    const std::string & s,
    const Fields & fields,
    ActivityId parent)
{
    if (on_start) {
        try {
            const char * type_str = activityTypeToString(type);
            on_start(act, s.c_str(), type_str, user_data);
        } catch (...) {
            // Silently ignore callback errors to avoid crashing Nix
        }
    }
}

void CallbackLogger::stopActivity(ActivityId act)
{
    if (on_stop) {
        try {
            on_stop(act, user_data);
        } catch (...) {
            // Silently ignore callback errors to avoid crashing Nix
        }
    }
}

void CallbackLogger::result(ActivityId act, ResultType type, const Fields & fields)
{
    if (!on_result)
        return;

    try {
        const char * result_type_str = resultTypeToString(type);
        size_t field_count = fields.size();

        // Build arrays of field data
        std::vector<int> field_types;
        std::vector<int64_t> int_values;
        std::vector<std::string> string_storage;
        std::vector<const char *> string_values;

        for (const auto & field : fields) {
            if (field.type == Logger::Field::tInt) {
                field_types.push_back(0);
                int_values.push_back(field.i);
                string_values.push_back(nullptr);
            } else if (field.type == Logger::Field::tString) {
                field_types.push_back(1);
                int_values.push_back(0);
                string_storage.push_back(field.s);
                string_values.push_back(string_storage.back().c_str());
            }
        }

        // Call the callback with raw data
        on_result(
            act,
            result_type_str,
            field_count,
            field_types.data(),
            int_values.data(),
            string_values.data(),
            user_data);
    } catch (...) {
        // Silently ignore callback errors to avoid crashing Nix
    }
}

} // namespace nix

extern "C" {

nix_err nix_set_logger_callbacks(
    nix_c_context * context,
    nix_activity_start_cb on_start,
    nix_activity_stop_cb on_stop,
    nix_activity_result_cb on_result,
    void * user_data)
{
    if (context)
        context->last_err_code = NIX_OK;
    try {
        // Create the callback logger
        auto callback_logger = std::make_unique<nix::CallbackLogger>(
            on_start, on_stop, on_result, user_data);

        // Replace the global logger
        nix::logger = std::move(callback_logger);

        return NIX_OK;
    }
    NIXC_CATCH_ERRS
}

} // extern "C"
