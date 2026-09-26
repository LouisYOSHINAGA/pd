# CZ-101 Envelope Analysis

`czenvrec/` の実機録音から DCO/DCW/DCA EG の特性を推定し、`eg.cpp` の実装を検証するためのスクリプト群。

- `czenvrec/presets/`: プリセット16音色（A4, note on 5 s / 周期 8 s）
- `czenvrec/20260922/`: DCA の rate / level sweep（DCW, DCO は level 0。note on 10 s / 周期 13 s）
- `czenvrec/20260925/`: 追加確認（Synth Bass の rate 再入力、初期化パッチの rate 24 attack と設定の入れ直し）→ `check_20260925.py`
- `czenvrec/20260926_2/`: rate 36 での Key Follow（attack/release とも同倍率）と Octave Range +1 の確認（Key Follow は鳴っている音程で決まる）→ `check_20260926_2.py`
- `czenvrec/20260926/`: DCA Key Follow 0〜9 × 鍵 C1〜C8（24〜108）→ `keyfollow2.py`（attack/release の rate code、速度比、sustain）、`keyfollow_model.py`（速度比 F = (12+k)/12 の補間モデルと検証、k の表 `dca_keyfollow_k.csv` を生成）
  - 20260922 と 20260925 B/C の初期化パッチは表示と内部状態が食い違っており、DCA が 22 code 速く、音色も Resonance III 相当だった。
    設定を入れ直した D はプリセットと同じ速度則（rate 24 → code 30）・純正弦波になる。

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
| `attack.py` | preset の立ち上がりを 1 ms 精度で測定（上昇も下降と同じ速度則であることの確認） |
| `fastrms.py` | 1 周期窓 RMS による高速な attack / release の測定（note off 後の rate 上限 code 104 の確認） |
| `dcasweep.py`, `volcurve2.py` | sweep 録音の読み込みと、sustain 値・遅い attack からの音量カーブ復元（1/12 octave/code, 下端は -0.5 LSB 相当の落ち込み） |

## 実装との比較
1. `harness\build.bat` で `harness/render.exe` をビルド（プラグインの `Voice`/`PD`/`EG` をそのまま使うオフラインレンダラ）。
2. `python render_all.py new` : 16 音色を CSV のパラメータでレンダリングし、DCA 包絡の誤差を表示 (`plots/vst_vs_cz_dca_new.png`)。
3. `python compare_dcw.py`, `python compare_dco.py` : DCW / DCO の時間変化を比較。
4. `python compare_sweep.py` : sweep 録音と比較（level 別 sustain 値、rate 別 attack 時間）。
   sweep 用パッチは同じ rate 値でプリセットより 22 code（約 6.7 倍）速いため、同じ rate code になる VST の rate に置き換えて比較する。

旧実装と比較する場合は `harness/build_old.bat` のコメントに従って旧ソースを `harness/old/` に展開してビルドし、`python render_all.py old` を実行する。
