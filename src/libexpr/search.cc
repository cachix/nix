#include "nix/expr/search.hh"
#include "nix/store/names.hh"
#include "nix/util/strings-inline.hh"

#include <algorithm>
#include <functional>

namespace nix {

void searchDerivations(
    EvalState & state,
    eval_cache::AttrCursor & cursor,
    const std::vector<std::regex> & regexes,
    const std::vector<std::regex> & excludeRegexes,
    SearchCallback callback,
    size_t recurseDepth
)
{
    bool continueSearch = true;

    std::function<void(eval_cache::AttrCursor &, const std::vector<Symbol> &, size_t)> visit;

    visit = [&](eval_cache::AttrCursor & cur, const std::vector<Symbol> & attrPath, size_t depth) {
        if (!continueSearch)
            return;

        auto attrPathS = state.symbols.resolve(attrPath);

        try {
            auto recurse = [&]() {
                for (const auto & attr : cur.getAttrs()) {
                    if (!continueSearch)
                        return;
                    auto cursor2 = cur.getAttr(attr);
                    auto attrPath2(attrPath);
                    attrPath2.push_back(attr);
                    visit(*cursor2, attrPath2, depth + 1);
                }
            };

            if (cur.isDerivation()) {
                DrvName drvName(cur.getAttr(state.s.name)->getString());

                auto metaAttr = cur.maybeGetAttr(state.s.meta);
                auto descAttr = metaAttr ? metaAttr->maybeGetAttr(state.s.description) : nullptr;
                auto description = descAttr ? descAttr->getString() : "";
                std::replace(description.begin(), description.end(), '\n', ' ');

                std::string attrPathStr = concatStringsSep(".", attrPathS);

                // Check exclude patterns (OR logic - excluded if any match)
                for (auto & regex : excludeRegexes) {
                    if (std::regex_search(attrPathStr, regex) || std::regex_search(drvName.name, regex)
                        || std::regex_search(description, regex))
                        return;
                }

                // Check include patterns (AND logic - must match all)
                bool found = true;
                for (auto & regex : regexes) {
                    if (!std::regex_search(attrPathStr, regex) && !std::regex_search(drvName.name, regex)
                        && !std::regex_search(description, regex)) {
                        found = false;
                        break;
                    }
                }

                if (found) {
                    SearchMatch match{attrPathStr, drvName.name, drvName.version, description};
                    continueSearch = callback(match);
                }
            } else if (depth < recurseDepth) {
                // Within auto-recurse depth - always recurse
                recurse();
            } else {
                // Beyond auto-recurse depth - check recurseForDerivations
                auto attr = cur.maybeGetAttr(state.s.recurseForDerivations);
                if (attr && attr->getBool()) {
                    recurse();
                }
            }

        } catch (EvalError &) {
            // Silently ignore eval errors (broken packages)
        }
    };

    visit(cursor, cursor.getAttrPath(), 0);
}

} // namespace nix
