#pragma once
///@file

#include "nix/util/logging.hh"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace nix {

enum class EvalEffect {
    EvaluatedFile,
    GetEnv,
    PathExists,
    ReadFile,
    HashFile,
    ReadFileType,
    ReadDir,
    FilterSource,
    CopySource,
};

inline std::string_view evalEffectName(EvalEffect effect)
{
    switch (effect) {
    case EvalEffect::EvaluatedFile:
        return "evaluated-file";
    case EvalEffect::GetEnv:
        return "get-env";
    case EvalEffect::PathExists:
        return "path-exists";
    case EvalEffect::ReadFile:
        return "read-file";
    case EvalEffect::HashFile:
        return "hash-file";
    case EvalEffect::ReadFileType:
        return "read-file-type";
    case EvalEffect::ReadDir:
        return "read-dir";
    case EvalEffect::FilterSource:
        return "filter-source";
    case EvalEffect::CopySource:
        return "copy-source";
    }
    std::unreachable();
}

/**
 * Report an evaluator dependency through the structured logger.
 *
 * The stable positional wire format is two or three strings:
 *
 *   [kind, subject, optional detail]
 *
 * Kinds currently emitted are `evaluated-file` (detail is `cached` or
 * `uncached`), `get-env`, `path-exists`, `read-file`, `hash-file` (detail is
 * the hash algorithm), `read-file-type`, `read-dir`, `filter-source` (detail
 * is the destination store path), and `copy-source` (detail is the
 * destination store path).
 */
inline void evalEffect(EvalEffect kind, std::string_view subject, std::optional<std::string_view> detail = std::nullopt)
{
    logger->evalEffect(evalEffectName(kind), subject, detail);
}

} // namespace nix
