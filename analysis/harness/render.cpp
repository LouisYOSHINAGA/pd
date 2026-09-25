// Offline renderer: drives the plugin's Voice/PD/EG classes directly.
// usage: render <params.txt> <out.raw>
// params.txt: "<lineSelect> <detuneRatio> <note> <gateSeconds> <totalSeconds>"
//             followed by 2 lines of kNumLineParams normalized values.
#include <cstdio>
#include <vector>
#include "voice.h"

using namespace Steinberg::Vst;

int main(int argc, char** argv) {
  if (argc < 3) { std::fprintf(stderr, "usage\n"); return 1; }
  FILE* f = std::fopen(argv[1], "r");
  if (!f) return 1;
  int lineSelect, note; double ratio, gate, total;
  if (std::fscanf(f, "%d %lf %d %lf %lf", &lineSelect, &ratio, &note, &gate, &total) != 5) return 1;
  Voice voice;
  voice.setLineSelect(static_cast<LineSelect>(lineSelect));
  voice.setDetuneRatio(ratio);
  for (int line = 0; line < 2; line++) {
    for (int k = 0; k < kNumLineParams; k++) {
      double v; if (std::fscanf(f, "%lf", &v) != 1) return 1;
      voice.setLineParam(line, k, v);
    }
  }
  std::fclose(f);
  const int n = static_cast<int>(total * kInternalSampleRate);
  const int off = static_cast<int>(gate * kInternalSampleRate);
  std::vector<float> out(n);
  voice.noteOn(0, note, 0);
  for (int i = 0; i < n; i++) {
    if (i == off) voice.noteOff();
    out[i] = voice.isActive() ? static_cast<float>(voice.generate(0.0)) : 0.0f;
  }
  FILE* o = std::fopen(argv[2], "wb");
  std::fwrite(out.data(), sizeof(float), out.size(), o);
  std::fclose(o);
  return 0;
}
