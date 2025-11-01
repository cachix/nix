#pragma once
/**
 * @internal
 * @file Internal implementation details for the logger C API
 *
 * This file is not part of the public C API and should not be included
 * by external code.
 */

#include "nix/util/logging.hh"
#include "nix_api_logger.h"

#include <memory>

namespace nix {

/**
 * @internal
 * Logger implementation that forwards activity events to C function pointers.
 *
 * This allows Rust/C code to observe Nix's build and other activities.
 */
class CallbackLogger : public Logger
{
private:
    nix_activity_start_cb on_start;
    nix_activity_stop_cb on_stop;
    nix_activity_result_cb on_result;
    void * user_data;

    /**
     * Convert ActivityType enum to string for callbacks
     */
    static const char * activityTypeToString(ActivityType type);

    /**
     * Convert ResultType enum to string for callbacks
     */
    static const char * resultTypeToString(ResultType type);

public:
    /**
     * Create a logger with callbacks
     */
    CallbackLogger(
        nix_activity_start_cb on_start = nullptr,
        nix_activity_stop_cb on_stop = nullptr,
        nix_activity_result_cb on_result = nullptr,
        void * user_data = nullptr);

    ~CallbackLogger() override = default;

    void startActivity(
        ActivityId act,
        Verbosity lvl,
        ActivityType type,
        const std::string & s,
        const Fields & fields,
        ActivityId parent) override;

    void stopActivity(ActivityId act) override;

    void result(ActivityId act, ResultType type, const Fields & fields) override;

    void log(Verbosity lvl, std::string_view s) override
    {
        // No-op: we only care about activities
    }

    void logEI(const ErrorInfo & ei) override
    {
        // No-op: we only care about activities
    }
};

} // namespace nix
