#include "core/search/SearchEngine.h"

#include "core/search/Pinyin.h"
#include "core/search/TextFold.h"

#include <algorithm>

namespace bei::core {

namespace {

using Term   = SearchEngine::Term;
using Clause = SearchEngine::Clause;
using Query  = SearchEngine::Query;
using Keys   = SearchEngine::Keys;

bool isSpace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }

// ---------------------------------------------------------------------------------------------
// Key building

/// Computes the search keys of `e`. With `snapshot`, also records the source texts for change detection.
void buildKeys(ItemEntry const& e, Keys& k, bool snapshot) {
    if (snapshot) {
        k.srcName    = e.displayName;
        k.srcEnglish = e.englishName;
        k.srcId      = e.key.id;
        k.srcTags    = e.tags;
        k.built      = true;
    }

    k.name.clear();
    k.units.clear();
    bool                                hasHan = false;
    std::vector<SearchEngine::NameUnit> units;
    text::forEachFolded(e.displayName, [&](char32_t cp, std::string_view folded) {
        SearchEngine::NameUnit u;
        u.begin  = static_cast<std::uint32_t>(k.name.size());
        u.length = static_cast<std::uint16_t>(folded.size());
        auto r   = pinyin::readingsOf(cp);
        for (std::size_t i = 0; i < r.count; ++i) u.readings[u.readingCount++] = r.ids[i];
        hasHan  = hasHan || r.count > 0;
        k.name += folded;
        if (u.length > 0 || u.readingCount > 0) units.push_back(u);
    });
    if (hasHan) k.units = std::move(units);

    k.english = text::fold(e.englishName);
    k.id      = text::fold(e.key.id);
    if (auto colon = k.id.find(':'); colon != std::string::npos) {
        k.ns     = k.id.substr(0, colon);
        k.idPath = k.id.substr(colon + 1);
    } else {
        k.ns     = "minecraft";
        k.idPath = k.id;
    }
    k.tags.clear();
    k.tags.reserve(e.tags.size());
    for (auto const& t : e.tags) k.tags.push_back(text::fold(t));
}

bool keysUpToDate(ItemEntry const& e, Keys const& k) {
    return k.srcName == e.displayName && k.srcEnglish == e.englishName && k.srcId == e.key.id && k.srcTags == e.tags;
}

// ---------------------------------------------------------------------------------------------
// Pinyin matching

/// Depth-first match of `term` against the name units starting at unit `i`, term offset `p`.
/// Each unit consumes either its folded literal or a non-empty prefix of one of its pinyin readings.
/// The term may end anywhere inside the last consumed unit. `visited` memoizes failed states.
class PinyinMatcher {
public:
    PinyinMatcher(Keys const& keys, std::string_view term)
    : mName(keys.name),
      mUnits(keys.units),
      mTerm(term),
      mVisited(scratch()) {
        mVisited.assign((mUnits.size() + 1) * (term.size() + 1), 0);
    }

    bool run() {
        for (std::size_t i = 0; i < mUnits.size(); ++i) {
            if (go(i, 0)) return true;
        }
        return false;
    }

private:
    bool go(std::size_t i, std::size_t p) {
        if (p == mTerm.size()) return true;
        if (i == mUnits.size()) return false;
        auto& seen = mVisited[i * (mTerm.size() + 1) + p];
        if (seen) return false;
        seen = 1;

        auto const& u    = mUnits[i];
        auto        rest = mTerm.substr(p);
        if (u.length > 0) {
            auto lit = mName.substr(u.begin, u.length);
            if (rest.size() <= lit.size()) {
                if (lit.starts_with(rest)) return true;
            } else if (rest.starts_with(lit) && go(i + 1, p + lit.size())) {
                return true;
            }
        }
        for (std::size_t r = 0; r < u.readingCount; ++r) {
            auto        syl    = pinyin::syllable(u.readings[r]);
            std::size_t common = 0;
            while (common < syl.size() && common < rest.size() && syl[common] == rest[common]) ++common;
            for (std::size_t j = 1; j <= common; ++j) {
                if (p + j == mTerm.size()) return true;
                if (go(i + 1, p + j)) return true;
            }
        }
        return false;
    }

    std::string_view                           mName;
    std::vector<SearchEngine::NameUnit> const& mUnits;
    std::string_view                           mTerm;
    std::vector<std::uint8_t>&                 mVisited;

    // Reused across calls so the hot filter loop does not allocate per entry.
    static std::vector<std::uint8_t>& scratch() {
        thread_local std::vector<std::uint8_t> buffer;
        return buffer;
    }
};

// ---------------------------------------------------------------------------------------------
// Evaluation

bool contains(std::string const& hay, std::string const& needle) { return hay.find(needle) != std::string::npos; }

bool termMatchesPositive(Term const& t, Keys const& k) {
    switch (t.kind) {
    case Term::Kind::Namespace:
        return k.ns.starts_with(t.text);
    case Term::Kind::Tag:
        return std::any_of(k.tags.begin(), k.tags.end(), [&](std::string const& tag) { return contains(tag, t.text); });
    case Term::Kind::Id:
        return contains(k.id, t.text);
    case Term::Kind::Text:
        break;
    }
    if (contains(k.name, t.text) || contains(k.english, t.text) || contains(k.idPath, t.text)) return true;
    if (t.text.find(':') != std::string::npos && contains(k.id, t.text)) return true;
    if (!t.quoted && !k.units.empty()) return PinyinMatcher{k, t.text}.run();
    return false;
}

bool evaluate(Query const& q, Keys const& k) {
    for (auto const& clause : q.clauses) {
        bool any = false;
        for (auto const& t : clause.terms) {
            if (termMatchesPositive(t, k) != t.negated) {
                any = true;
                break;
            }
        }
        if (!any) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------
// Incremental refinement: does every result of `next` necessarily match `prev`?

bool termImplies(Term const& n, Term const& o) {
    if (n.kind != o.kind || n.negated != o.negated || n.quoted != o.quoted) return false;
    // Every matcher is prefix-closed: if "abc" matches then "ab" matches (substring, namespace prefix
    // and the pinyin walk alike). Negation flips that, so negated terms must be identical.
    return n.negated ? n.text == o.text : n.text.starts_with(o.text);
}

bool clauseImplies(Clause const& n, Clause const& o) {
    return std::all_of(n.terms.begin(), n.terms.end(), [&](Term const& nt) {
        return std::any_of(o.terms.begin(), o.terms.end(), [&](Term const& ot) { return termImplies(nt, ot); });
    });
}

bool refines(Query const& next, Query const& prev) {
    return std::all_of(prev.clauses.begin(), prev.clauses.end(), [&](Clause const& oc) {
        return std::any_of(next.clauses.begin(), next.clauses.end(), [&](Clause const& nc) {
            return clauseImplies(nc, oc);
        });
    });
}

} // namespace

// -------------------------------------------------------------------------------------------------

SearchEngine::Query SearchEngine::parse(std::string_view raw) {
    std::string const s = text::fold(raw, /*stripFormatting=*/false);
    Query             q;
    bool              pendingOr = false;
    std::size_t       i         = 0;
    while (i < s.size()) {
        char c = s[i];
        if (isSpace(c)) {
            ++i;
            continue;
        }
        if (c == '|') {
            pendingOr = true;
            ++i;
            continue;
        }
        Term t;
        if (s[i] == '-') t.negated = true, ++i;
        if (i < s.size()) {
            switch (s[i]) {
            case '@':
                t.kind = Term::Kind::Namespace, ++i;
                break;
            case '#':
                t.kind = Term::Kind::Tag, ++i;
                break;
            case '$':
                t.kind = Term::Kind::Id, ++i;
                break;
            default:
                break;
            }
        }
        if (i < s.size() && s[i] == '"') {
            t.quoted   = true;
            auto close = s.find('"', i + 1);
            if (close == std::string::npos) close = s.size(); // unterminated: runs to the end
            t.text = s.substr(i + 1, close - i - 1);
            i      = std::min(close + 1, s.size());
        } else {
            auto start = i;
            while (i < s.size() && !isSpace(s[i]) && s[i] != '|') ++i;
            t.text = s.substr(start, i - start);
        }
        if (t.text.empty()) { // still being typed ("-", "@", "\"\""): ignore it and any '|' before it
            pendingOr = false;
            continue;
        }
        if (pendingOr && !q.clauses.empty()) q.clauses.back().terms.push_back(std::move(t));
        else q.clauses.push_back(Clause{{std::move(t)}});
        pendingOr = false;
    }
    return q;
}

void SearchEngine::setItems(std::vector<ItemEntry> const* items, std::uint64_t revision) {
    if (mHasRevision && items == mItems && revision == mRevision && items && items->size() == mKeys.size()) return;
    setItems(items);
    mHasRevision = true;
    mRevision    = revision;
}

void SearchEngine::setItems(std::vector<ItemEntry> const* items) {
    mHasRevision = false;
    bool changed = items != mItems;
    mItems       = items;
    if (!items) {
        changed = changed || !mKeys.empty();
        mKeys.clear();
    } else {
        if (mKeys.size() != items->size()) {
            changed = true;
            mKeys.resize(items->size());
            // Positions may have shifted, so every entry is re-validated below.
        }
        for (std::size_t i = 0; i < items->size(); ++i) {
            auto const& e = (*items)[i];
            auto&       k = mKeys[i];
            if (k.built && keysUpToDate(e, k)) continue;
            buildKeys(e, k, /*snapshot=*/true);
            ++mKeyBuildCount;
            changed = true;
        }
    }
    if (changed) {
        mHasLast = false;
        mLastResult.clear();
    }
}

bool SearchEngine::matches(ItemEntry const& entry, std::string_view query) const {
    Keys k;
    buildKeys(entry, k, /*snapshot=*/false);
    return evaluate(parse(query), k);
}

std::vector<std::size_t> SearchEngine::filter(std::string_view query) const {
    std::vector<std::size_t> out;
    mLastScanCount = 0;
    if (!mItems || mKeys.size() != mItems->size()) {
        // No items, or the list was resized behind our back without setItems(): nothing safe to scan.
        mHasLast = false;
        return out;
    }

    Query q = parse(query);
    if (q.clauses.empty()) {
        out.resize(mItems->size());
        for (std::size_t i = 0; i < out.size(); ++i) out[i] = i;
    } else if (mHasLast && refines(q, mLastQuery)) {
        out.reserve(mLastResult.size());
        for (auto idx : mLastResult) {
            if (evaluate(q, mKeys[idx])) out.push_back(idx);
        }
        mLastScanCount = mLastResult.size();
    } else {
        for (std::size_t i = 0; i < mKeys.size(); ++i) {
            if (evaluate(q, mKeys[i])) out.push_back(i);
        }
        mLastScanCount = mKeys.size();
    }
    mHasLast    = true;
    mLastQuery  = std::move(q);
    mLastResult = out;
    return out;
}

} // namespace bei::core
