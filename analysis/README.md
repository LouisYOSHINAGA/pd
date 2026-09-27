# CZ-101 Envelope Analysis

`czenvrec/` の実機録音から DCO/DCW/DCA EG の特性を推定し、`eg.cpp` の実装を検証するためのスクリプト群。
録音データはcommitせず[Google Drive](https://drive.google.com/drive/folders/1nvWJIW0wf2pGReZSfiPwcmNJbYOr8532?usp=sharing)上にアップロードする。

- `czenvrec/presets/`: プリセット16音色（A4, note on 5 s / 周期 8 s）
- `czenvrec/20260922/`: DCA の rate / level sweep（DCW, DCO は level 0。note on 10 s / 周期 13 s）
- `czenvrec/20260925/`: 追加確認（Synth Bass の rate 再入力、初期化パッチの rate 24 attack と設定の入れ直し）→ `check_20260925.py`
- `czenvrec/20260927/`: sustain 到達前の離鍵（到達前・sustain なしは最終 step へ直行）→ `check_20260927.py`
- `czenvrec/20260926_2/`: rate 36 での Key Follow（attack/release とも同倍率）と Octave Range +1 の確認（Key Follow は鳴っている音程で決まる）→ `check_20260926_2.py`
- `czenvrec/20260926/`: DCA Key Follow 0〜9 × 鍵 C1〜C8（24〜108）→ `keyfollow2.py`（attack/release の rate code、速度比、sustain）、`keyfollow_model.py`（速度比 F = (12+k)/12 の補間モデルと検証、k の表 `dca_keyfollow_k.csv` を生成）
  - 20260922 と 20260925 B/C の初期化パッチは表示と内部状態が食い違っており、DCA が 22 code 速く、音色も Resonance III 相当だった。
    設定を入れ直した D はプリセットと同じ速度則（rate 24 → code 30）・純正弦波になる。
- `czenvrec/20260927_2/`: DCW Key Follow × 鍵（DCW rate 24 で 99 まで上昇）→ `dcwkf.py`（倍音照合による dcw(t)）、`dcwkf2.py`（上昇が止まる level code）、`dcwkf_presets.py`（モデルをプリセットで確認）
  - Key Follow で DCW の速度は変わらない。高音での DCW の制限は 2 種類: KF によらない頭打ち（127 - code ≈ 0.024*f[Hz]、Violin からは DCO envelope を含む発振周波数で決まるらしい）と、KF 1〜9 の level からの引き算（発音の鍵で決まる）。level 99 では KF 1〜7 の引き算は頭打ちに隠れて見えない。
- `czenvrec/20260927_3/`: DCW level 50（鍵 96, KF 0〜9 と 鍵 72, KF 9）と level 75（鍵 84, KF 1〜9）→ `check_20260927_3.py`（KF 0 は頭打ちのみ、KF 1〜9 は KF に応じた量が level から引かれる）
- `czenvrec/20260927_4/`: DCO で音程を +12 半音上げた場合など → `check_20260927_4.py`（頭打ちはその時点の発振音程で、KF の引き算は鍵で決まる。DCO rate 20 の glide は 2.75 半音/s で eg.cpp のモデル 3.56 より遅い。頭打ちは出力にかかり、内部の値が上限を下回るまで張り付く）
  - `dcwkf_model.py`: 全測定点から KF の引き算表 `dcw_keyfollow_s.csv`（KF × 鍵 36〜96、`eg.cpp` の `kDcwKeyFollowS`）と上限の点（`kDcwLimitCode`）を作る。引き算は s(KF) × g(鍵) の形で全 60 点に 1.5 code 以内で合う。
- `czenvrec/20260928/`, `20260928_2/`: KF 3, 5, 7, 9 × 鍵 60〜90（DCW level 50）、KF 9 × 鍵 90, 93（level 99）、KF 0 × 鍵 60〜93（rate 30, level 99）→ `check_20260928.py`（KF 3 の "note_69" は実際には鍵 66）。上限は C4 から既にかかる（C4 で 123.9 code）。
- 音程: この CZ-101 はどの鍵でも約 +9.6 cent 高い（A4 ≈ 442.4 Hz、20260927_2 の KF 0 で鍵 36〜96 を測定、ばらつき ±0.5 cent）。これを差し引くと DCO level 66 / 32 はちょうど +12.00 / +4.00 半音。
- `czenvrec/20260927_5/`: DCO rate 10〜60（level 66 = +12 半音）と level 32（+4 半音）、鍵 72、DCW 0 → `check_20260927_5.py`（glide は半音に対して直線、code = 127r/99・1 半音 14 × 2^16 単位で rate 10〜50 が 0.5% 以内、rate 60 は +5%）

## 参考資料
- Michael Rickard, "Casio CZ Envelopes"（非公式、[Web Archive](https://web.archive.org/web/20201111190255/https://www.kasploosh.com/cz/13466-envelopes/)）の PDF "Casio CZ MIDI Specification - Envelope Data"：パネル値 α から sysex 値 β への変換（整数演算）。
  | | rate | level |
  |---|---|---|
  | DCO | β = 127α/99 | α ≤ 63: β = α、α > 63: β = α + 4 |
  | DCW | β = 119α/99 + 8 | β = 127α/99 |
  | DCA | β = 119α/99 | α = 0: 0、それ以外 β = α + 28 |
  - 録音から求めたモデルとの対応：DCA level は一致。DCA rate はチップの code = β + 2。DCW rate は DCA + 8 だが、+8 code はちょうど 2 倍速なので、DCW の内部単位が DCA の半分なら録音と一致する。DCO level の半音換算（β < 64: β/8、β ≥ 68: 2(β - 64)）とも矛盾しない。DCW level は切り捨て（`eg.cpp` もこれに合わせた）。
  - DCO rate は 127/99 倍で、DCA/DCW の 119/99 倍とは rate の間隔が異なる。20260927_5 の録音で、チップの code = β（オフセットなし）、1 半音 = 14 × 2^16 単位と確認した（`eg.cpp` も変更）。

必要なもの: Python 3 + numpy / scipy / matplotlib、録音 (`czenvrec/*.wav`, `CZ101PresetParam .csv`)。
出力 (`plots/`, `render/`) は git 管理外。

## 推定手順
| スクリプト | 内容 |
|---|---|
| `rmsenv.py` | 各 take を位置合わせし、RMS 包絡 (dB) を平均。detune による beat は beat 周期の窓で除去 |
| `czeg.py`, `dcamodel.py`, `fitdca.py` | チップ EG モデル (35 kHz, step=(8+(n&7))<<(n>>3), n=round(1.25*rate)) で DCA 包絡を予測し録音に fit |
| `volcurve.py` | 減衰中の dB を accumulator 位置に対して描画 (0.495 dB/level code の確認) |
| `pdosc.py`, `harm.py`, `plateau.py`, `dcwtrack.py` | 倍音パワーを発振器モデルと照合して DCW 値を推定 (sustain 値・時間変化) |
| `dcwdepth.py` | DCW の level code と位相歪みの深さの関係（20260927_2 で鍵ごとの出力特性と同時に fit。深さ = 0.97 × code/127、全鍵で一致・直線） |
| `pitch.py`, `pitch2.py` | zero-cross による DCO pitch glide の測定 |
| `attack.py` | preset の立ち上がりを 1 ms 精度で測定（上昇も下降と同じ速度則であることの確認） |
| `compare_attack.py` | 16 音色の立ち上がりを実機と VST で比較（-40 dB から -20/-10/-6/-3/-1 dB までの時間） |
| `attack_start.py` | 20260922 の遅い attack を音量カーブ込みで fit し、立ち上がりの開始点を推定（level code 0 から開始、dB に対して直線＝振幅は指数的） |
| `trumpet_attack.py` | Trumpet の最初の DCA 段の速さと開始点を実機と VST で比較（VST が約 7% 速い、どちらも code 0 から） |
| `render_ab.py` | 16 音色の実機録音と VST を同条件・音量合わせで書き出す（`render/ab/`、聴き比べ用、commit しない） |
| `gate.py` | 実機の発音長（attack 開始 → release 開始）と MIDI の gate の差（note off の反応遅れ、約 +12 ms）の測定 |
| `fastrms.py` | 1 周期窓 RMS による高速な attack / release の測定（note off 後の rate 上限 code 104 の確認） |
| `dcasweep.py`, `volcurve2.py` | sweep 録音の読み込みと、sustain 値・遅い attack からの音量カーブ復元（1/12 octave/code, 下端は -0.5 LSB 相当の落ち込み） |

## 実装との比較
1. `harness\build.bat` で `harness/render.exe` をビルド（プラグインの `Voice`/`PD`/`EG` をそのまま使うオフラインレンダラ）。
2. `python render_all.py new` : 16 音色を CSV のパラメータでレンダリングし、DCA 包絡の誤差を表示 (`plots/vst_vs_cz_dca_new.png`)。
3. `python compare_dcw.py`, `python compare_dco.py` : DCW / DCO の時間変化を比較。
   `python compare_dcw_cal.py`: 20260927_2 で推定の偏りを確認したうえで、DCW Key Follow の有無で比較（深さを 0.97 に直してからは偏りはほぼ 0）。
4. `python compare_sweep.py` : sweep 録音と比較（level 別 sustain 値、rate 別 attack 時間）。
   sweep 用パッチは同じ rate 値でプリセットより 22 code（約 6.7 倍）速いため、同じ rate code になる VST の rate に置き換えて比較する。

旧実装と比較する場合は `harness/build_old.bat` のコメントに従って旧ソースを `harness/old/` に展開してビルドし、`python render_all.py old` を実行する。
