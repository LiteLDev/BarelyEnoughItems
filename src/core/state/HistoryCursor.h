#pragma once

#include <optional>
#include <string>
#include <vector>

namespace bei::core {

/// Shell-style Up/Down navigation through a newest-first history list (Config::searchHistory).
///
/// older() steps back in time, newer() forward; stepping forward past the newest entry returns
/// the text that was in the box before navigation started (the draft). Entries equal to the text
/// currently shown or to the draft are skipped, so every key press changes the text: Up right
/// after submitting "oak" goes to the search before it, and Down from there returns to "oak".
/// Call reset() whenever the user edits the text or the box loses focus.
class HistoryCursor {
public:
    /// The next older entry to show instead of `current`, or nullopt if there is none.
    [[nodiscard]] std::optional<std::string> older(std::vector<std::string> const& history, std::string const& current);
    /// The next newer entry (or the draft) to show instead of `current`, or nullopt when not
    /// navigating.
    [[nodiscard]] std::optional<std::string> newer(std::vector<std::string> const& history, std::string const& current);
    void                                     reset() noexcept { mIndex = -1; }
    /// Index into the history of the entry shown, or -1 when showing the user's own text.
    [[nodiscard]] int index() const noexcept { return mIndex; }

private:
    int         mIndex = -1;
    std::string mDraft;
};

} // namespace bei::core
