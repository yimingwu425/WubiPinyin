# Third-party data notices

The WubiPinyin configuration does not copy an upstream word table into this
repository. A distributable bundle must stage the exact files in
`sources.lock.json`, verify their SHA-256 values, and carry each source
repository's `LICENSE` and `AUTHORS` files with the bundle.

## Rime Wubi

- Source: `rime/rime-wubi` at
  `152a0d3f3efe40cae216d1e3b338242446848d07`
- Used file: `wubi86.dict.yaml`
- License: GNU Lesser General Public License v3.0
- Attribution: derived from ibus-table by Yu Yuwei and based on Wang Yongmin's
  original work, as recorded in the upstream `AUTHORS` file.

## Rime Pinyin Simplified

- Source: `rime/rime-pinyin-simp` at
  `0c6861ef7420ee780270ca6d993d18d4101049d0`
- Used file: `pinyin_simp.dict.yaml`
- License: Apache License 2.0
- Attribution: derived from Android Pinyin IME, as recorded in the upstream
  `AUTHORS` file.

The configuration files in `rime/` are WubiPinyin additions. They do not
change either upstream data file. Do not substitute a branch head for a locked
revision in a release.
