#!/usr/bin/env sh
set -eu

data_dir=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
schema="$data_dir/rime/hybrid_auto.schema.yaml"

require_literal() {
  file=$1
  text=$2
  if ! grep -Fq -- "$text" "$file"; then
    printf 'missing expected text in %s: %s\n' "$file" "$text" >&2
    exit 1
  fi
}

reject_pattern() {
  file=$1
  pattern=$2
  if grep -Eq -- "$pattern" "$file"; then
    printf 'unexpected schema feature in %s: %s\n' "$file" "$pattern" >&2
    exit 1
  fi
}

test -f "$schema"
require_literal "$schema" 'schema_id: hybrid_auto'
require_literal "$schema" '- table_translator@wubi'
require_literal "$schema" '- script_translator@pinyin'
require_literal "$schema" '- hybrid_filter@hybrid'
require_literal "$schema" 'max_code_length: 0'
require_literal "$schema" 'auto_select: false'
require_literal "$schema" 'options: [ hybrid_auto, hybrid_wubi, hybrid_pinyin ]'
require_literal "$schema" 'accept: Control+Shift+a'
require_literal "$schema" 'accept: Control+Shift+w'
require_literal "$schema" 'accept: Control+Shift+p'
require_literal "$schema" 'wubi_candidate_types: [ table, user_table ]'
require_literal "$schema" 'pinyin_candidate_types: [ phrase, user_phrase, sentence ]'
require_literal "$schema" 'hybrid_auto_wubi: wubi'
require_literal "$schema" 'hybrid_auto_pinyin: pinyin'
require_literal "$schema" 'passthrough_unclassified: true'
require_literal "$schema" 'ranking_window: 64'

reject_pattern "$schema" '^[[:space:]]*-[[:space:]]*(reverse_lookup|reverse_lookup_translator|reverse_lookup_filter)'
reject_pattern "$schema" '(^|[[:space:]])(abbrev|derive)/'
reject_pattern "$schema" '(double_pinyin|fuzzy)'

require_literal "$data_dir/rime/hybrid_auto_wubi.dict.yaml" '- wubi86'
require_literal "$data_dir/rime/hybrid_auto_wubi.dict.yaml" '- hybrid_auto_wubi_user'
require_literal "$data_dir/rime/hybrid_auto_pinyin.dict.yaml" '- pinyin_simp'
require_literal "$data_dir/rime/hybrid_auto_pinyin.dict.yaml" '- hybrid_auto_pinyin_user'
require_literal "$data_dir/sources.lock.json" '152a0d3f3efe40cae216d1e3b338242446848d07'
require_literal "$data_dir/sources.lock.json" '0c6861ef7420ee780270ca6d993d18d4101049d0'

printf '%s\n' 'WubiPinyin Rime schema validation passed.'
