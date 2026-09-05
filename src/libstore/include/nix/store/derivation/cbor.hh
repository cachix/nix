#pragma once

#include "nix/store/derivations.hh"

namespace nix::derivation {

inline constexpr unsigned expectedCborVersion = 1;

std::string toCbor(const Full & drv);

std::string toCbor(const std::map<StorePath, Full> & drvs);

Full parseCbor(std::string_view bytes, const ExperimentalFeatureSettings & xpSettings = experimentalFeatureSettings);

} // namespace nix::derivation
