# CZ-101 Envelope Analysis

`czenvrec/` の実機録音（CZ-101 プリセット16音色, A4, note on 5 s / 周期 8 s）から
DCO/DCW/DCA EG の特性を推定し、`eg.cpp` の実装を検証するためのスクリプト群。

必要なもの: Python 3 + numpy / scipy / matplotlib、録音 (`czenvrec/*.wav`, `CZ101PresetParam .csv`)。
出力 (`plots/`, `render/`) は git 管理外。

## 推定手順
| スクリプト | 内容 |
|---|---|
| `rmsenv.py` | 各 take を位置合わせし、RMS 包絡 (dB) を平均。detune による beat は beat 周期の窓で除去 |
| `czeg.py`, `dcamodel.py`, `fitdca.py` | チップ EG モデル (35 kHz, step=(8+(n&7))<<(n>>3), n=round(1.25*rate)) で DCA 包絡を予測し録音に fit |
| `volcurve.py` | 減衰中の dB を accumulator 位置に対して描画 (0.495 dB/level code の確認) |
| `pdosc.py`, `harm.py`, `plateau.py`, `dcwtrack.py` | 倍音パワーを発振器モデルと照合して DCW 値を推定 (sustain 値・時間変化) |
| `pitch.py`, `pitch2.py` | zero-cross による DCO pitch glide の測定 |

## 実装との比較
1. `harness\build.bat` で `harness/render.exe` をビルド（プラグインの `Voice`/`PD`/`EG` をそのまま使うオフラインレンダラ）。
2. `python render_all.py new` : 16 音色を CSV のパラメータでレンダリングし、DCA 包絡の誤差を表示 (`plots/vst_vs_cz_dca_new.png`)。
3. `python compare_dcw.py`, `python compare_dco.py` : DCW / DCO の時間変化を比較。

旧実装と比較する場合は `harness/build_old.bat` のコメントに従って旧ソースを `harness/old/` に展開してビルドし、`python render_all.py old` を実行する。
