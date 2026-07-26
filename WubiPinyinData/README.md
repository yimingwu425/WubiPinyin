# WubiPinyin Rime data

This directory is the WubiPinyin-owned Rime data package. Its `rime/` contents
are installed as a single flat group into the Rime source directory. The
installer must also stage the two locked upstream dictionary files there:

- `wubi86.dict.yaml` from `rime-wubi`
- `pinyin_simp.dict.yaml` from `rime-pinyin-simp`

The schema has two translators on the same `abc` composition segment:

- `table_translator@wubi` reads `hybrid_auto_wubi`, which imports Wubi86 plus
  the broker-managed Wubi user entries.
- `script_translator@pinyin` reads `hybrid_auto_pinyin`, which imports full
  Pinyin plus the broker-managed Pinyin user entries.

`speller` has no algebra section. This intentionally permits only the normal
full-Pinyin spellings compiled from the pinned data: no double-Pinyin mapping,
fuzzy derivation, or initial-letter abbreviation. `max_code_length: 0` and
`auto_select: false` prevent four-code Wubi and unique-candidate auto commit.
The existing `express_editor` supplies the intended `Space`, number, `Enter`,
and `Esc` semantics.

## HybridFilter contract

`hybrid_filter@hybrid` is an integration point for the WubiPinyin Rime
extension. It must be registered under the component name `hybrid_filter`.
The filter reads the `hybrid` block and the three radio options set by
`Ctrl+Shift+A`, `Ctrl+Shift+W`, and `Ctrl+Shift+P`.

For each candidate it must infer the source from `wubi_candidate_types` and
`pinyin_candidate_types`. A `completion` candidate has no route-specific type,
so the filter must instead use its Phrase language and `source_languages`.
It attaches the configured source-mask bit and merges same-text candidates by
OR-ing those bits. In `hybrid_auto` it ranks the merged candidates; in
`hybrid_wubi` or `hybrid_pinyin` it removes candidates that do not carry the
selected bit. Unclassified candidates, including punctuation, pass through.
The output candidate types are `hybrid_wubi`, `hybrid_pinyin`, and
`hybrid_both`; each remains a `UniquifiedCandidate`, so Rime's learning path
still resolves its original Phrase. Candidate transport should expose the
`CandidateSourceMask` (`1` Wubi, `2` Pinyin, `3` both), without encoding the
source into the committed text.

The filter ranks one bounded initial source-candidate window per update.
`hybrid/ranking_window` defaults to `64` and is capped at `256`; later
candidates are fetched lazily in their original order as the candidate menu
needs them. This keeps the keystroke path bounded without truncating long
candidate lists.

## Broker-managed dictionaries

The two `*_user.dict.yaml` files are empty, valid templates. SQLite remains
the owner of user entries. During maintenance the Broker serializes enabled
rows as `text<TAB>code<TAB>weight` into the route-specific staging file and
atomically replaces the deployed copy before triggering Rime deployment.
Rime LevelDB user dictionaries remain separate and learn only through normal
candidate selection; the Broker must not write to them directly.

See `THIRD_PARTY_NOTICES.md` and `sources.lock.json` before changing a source
revision or shipping any upstream dictionary data.
