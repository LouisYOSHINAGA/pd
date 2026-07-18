#include "editor.h"

#include <cmath>
#include <cstdio>
#include <string>

#include "vstgui/lib/cframe.h"
#include "vstgui/lib/cgradient.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/controls/cknob.h"
#include "vstgui/lib/controls/coptionmenu.h"
#include "vstgui/lib/controls/csegmentbutton.h"
#include "vstgui/lib/controls/ctextlabel.h"
#include "controller.h"

namespace Steinberg {
namespace Vst {

using namespace VSTGUI;

namespace {

constexpr int kEditorWidth = 1000;
constexpr int kEditorHeight = 540;

// palette: dark, flat, one warm accent (CZ-style orange), phosphor scope
const CColor kColBg(18, 19, 24);
const CColor kColPanel(26, 28, 34);
const CColor kColPanelLight(33, 36, 44);
const CColor kColText(205, 208, 214);
const CColor kColTextDim(122, 126, 138);
const CColor kColAccent(255, 122, 44);
const CColor kColKnobRing(58, 62, 74);
const CColor kColScopeBg(10, 12, 14);
const CColor kColScopeGrid(28, 34, 38);
const CColor kColScopeTrace(64, 232, 130);
const CColor kColDcoAccent(238, 178, 92);
const CColor kColDcwAccent(92, 196, 210);
const CColor kColDcaAccent(150, 206, 120);

const CColor kEgAccents[] = {kColDcoAccent, kColDcwAccent, kColDcaAccent};
const char* const kEgTitles[] = {"DCO ENVELOPE / PITCH", "DCW ENVELOPE / TIMBRE",
                                 "DCA ENVELOPE / AMP"};

const std::vector<std::string> kWaveformEntries = {
  "1 Saw Tooth", "2 Square", "3 Pulse", "4 Double Sine", "5 Saw Pulse",
  "6 Resonance I", "7 Resonance II", "8 Resonance III",
};

SharedPointer<CFontDesc> makeFont(double size, bool bold) {
  return makeOwned<CFontDesc>("Segoe UI", size, bold ? kBoldFace : kNormalFace);
}

}  // namespace

// ---------------------------------------------------------------------------
// OscilloscopeView

OscilloscopeView::OscilloscopeView(const CRect& size, PDController* controller)
    : CView(size), controller_(controller) {
}

bool OscilloscopeView::attached(CView* parent) {
  bool result = CView::attached(parent);
  timer_ = makeOwned<CVSTGUITimer>([this](CVSTGUITimer*) { invalid(); }, 33);
  return result;
}

bool OscilloscopeView::removed(CView* parent) {
  timer_ = nullptr;
  return CView::removed(parent);
}

void OscilloscopeView::draw(CDrawContext* context) {
  const CRect r = getViewSize();
  context->setDrawMode(kAntiAliasing);

  context->setFillColor(kColScopeBg);
  context->drawRect(r, kDrawFilled);

  // grid: center line + quarter divisions
  context->setFrameColor(kColScopeGrid);
  context->setLineWidth(1.0);
  for (int i = 1; i < 4; i++) {
    CCoord x = r.left + r.getWidth() * i / 4.0;
    context->drawLine(std::make_pair(CPoint(x, r.top), CPoint(x, r.bottom)));
  }
  CCoord midY = r.top + r.getHeight() / 2.0;
  context->drawLine(std::make_pair(CPoint(r.left, midY), CPoint(r.right, midY)));

  controller_->copyScopeData(frame_);
  if (frame_.size() < 2) {
    setDirty(false);
    return;
  }

  // rising-edge trigger in the first half for a stable image
  int total = static_cast<int>(frame_.size());
  int window = total / 2;
  int trigger = 0;
  for (int i = 1; i < window; i++) {
    if (frame_[i - 1] < 0.0f && frame_[i] >= 0.0f) {
      trigger = i;
      break;
    }
  }

  auto pointAt = [&](int i) {
    double t = static_cast<double>(i) / (window - 1);
    double v = frame_[trigger + i];
    if (v > 1.0) v = 1.0;
    if (v < -1.0) v = -1.0;
    return CPoint(r.left + t * r.getWidth(), midY - v * (r.getHeight() / 2.0 - 3.0));
  };

  if (CGraphicsPath* path = context->createGraphicsPath()) {
    path->beginSubpath(pointAt(0));
    for (int i = 1; i < window; i++) {
      path->addLine(pointAt(i));
    }
    // soft glow pass + crisp trace pass
    CColor glow = kColScopeTrace;
    glow.alpha = 70;
    context->setFrameColor(glow);
    context->setLineWidth(3.4);
    context->drawGraphicsPath(path, CDrawContext::kPathStroked);
    context->setFrameColor(kColScopeTrace);
    context->setLineWidth(1.4);
    context->drawGraphicsPath(path, CDrawContext::kPathStroked);
    path->forget();
  }

  // frame border
  context->setFrameColor(kColPanelLight);
  context->setLineWidth(1.0);
  context->drawRect(r, kDrawStroked);

  setDirty(false);
}

// ---------------------------------------------------------------------------
// PDEditor

PDEditor::PDEditor(void* controller) : VSTGUIEditor(controller) {
  ViewRect viewRect(0, 0, kEditorWidth, kEditorHeight);
  setRect(viewRect);
}

void PDEditor::addLabel(CViewContainer* parent, const CRect& rect, const char* text,
                        const CColor& color, double fontSize, bool bold,
                        CHoriTxtAlign align) {
  CTextLabel* label = new CTextLabel(rect, text);
  label->setFont(makeFont(fontSize, bold));
  label->setFontColor(color);
  label->setBackColor(kTransparentCColor);
  label->setFrameColor(kTransparentCColor);
  label->setHoriAlign(align);
  label->setStyle(CParamDisplay::kNoFrame);
  parent->addView(label);
}

void PDEditor::addKnob(CViewContainer* parent, const CRect& rect, ParamID tag,
                       const CColor& coronaColor, bool bipolar, const char* tooltip) {
  int32_t drawStyle = CKnob::kCoronaDrawing | CKnob::kCoronaOutline;
  if (bipolar) {
    drawStyle |= CKnob::kCoronaFromCenter;
  }
  CKnob* knob = new CKnob(rect, this, tag, nullptr, nullptr, CPoint(0, 0), drawStyle);
  knob->setCoronaColor(coronaColor);
  knob->setColorShadowHandle(kColKnobRing);
  knob->setColorHandle(kColText);
  knob->setHandleLineWidth(2.0);
  knob->setCoronaInset(3.0);
  knob->setTooltipText(tooltip);
  parent->addView(knob);
  bindings_[tag] = Binding{knob, 0};
}

void PDEditor::addMenu(CViewContainer* parent, const CRect& rect, ParamID tag,
                       const std::vector<std::string>& entries) {
  COptionMenu* menu = new COptionMenu(rect, this, tag);
  for (const std::string& entry : entries) {
    menu->addEntry(entry.c_str());
  }
  menu->setFont(makeFont(11, false));
  menu->setFontColor(kColText);
  menu->setBackColor(kColPanelLight);
  menu->setFrameColor(kColKnobRing);
  menu->setHoriAlign(kLeftText);
  menu->setStyle(CParamDisplay::kRoundRectStyle);
  menu->setRoundRectRadius(3.0);
  parent->addView(menu);
  bindings_[tag] = Binding{menu, static_cast<int32>(entries.size())};
}

void PDEditor::addSegmentButton(CViewContainer* parent, const CRect& rect, ParamID tag,
                                const std::vector<std::string>& segments) {
  CSegmentButton* button = new CSegmentButton(rect, this, tag);
  for (const std::string& name : segments) {
    CSegmentButton::Segment segment;
    segment.name = name.c_str();
    button->addSegment(std::move(segment));
  }
  button->setFont(makeFont(11, true));
  button->setTextColor(kColTextDim);
  button->setTextColorHighlighted(CColor(20, 20, 24));
  button->setFrameColor(kColKnobRing);
  button->setFrameWidth(1.0);
  button->setRoundRadius(4.0);
  button->setGradient(CGradient::create(0, 1, kColPanelLight, kColPanelLight));
  button->setGradientHighlighted(CGradient::create(0, 1, kColAccent, kColAccent));
  parent->addView(button);
  bindings_[tag] = Binding{button, 0};
}

void PDEditor::buildHeader(CFrame* frame) {
  addLabel(frame, CRect(24, 8, 110, 52), "PD", kColText, 34, true);
  addLabel(frame, CRect(112, 26, 460, 44), "PHASE DISTORTION SYNTHESIZER", kColTextDim, 10);
  addLabel(frame, CRect(112, 12, 460, 26), "CZ SERIES TRIBUTE", kColAccent, 9, true);

  addLabel(frame, CRect(598, 50, 646, 62), "VOLUME", kColTextDim, 8, false, kCenterText);
  addKnob(frame, CRect(604, 12, 640, 48), kParamVolume, kColAccent, false, "Volume");

  OscilloscopeView* scope = new OscilloscopeView(
    CRect(672, 8, 976, 60), static_cast<PDController*>(getController())
  );
  frame->addView(scope);
}

void PDEditor::buildGlobalRow(CFrame* frame) {
  addLabel(frame, CRect(24, 76, 120, 90), "LINE SELECT", kColTextDim, 9, true);
  addSegmentButton(frame, CRect(24, 92, 288, 122), kParamLineSelect,
                   {"1", "2", "1+1'", "1+2'"});

  addLabel(frame, CRect(320, 76, 400, 90), "KEY ASSIGN", kColTextDim, 9, true);
  addSegmentButton(frame, CRect(320, 92, 452, 122), kParamMonoPoly, {"POLY", "MONO"});

  addLabel(frame, CRect(492, 76, 560, 90), "DETUNE", kColTextDim, 9, true);
  const struct {
    ParamID tag;
    const char* label;
    const char* tooltip;
  } detunes[] = {
    {kParamDetuneOctave, "OCT", "Detune Octave"},
    {kParamDetuneNote, "NOTE", "Detune Note"},
    {kParamDetuneFine, "FINE", "Detune Fine"},
  };
  double x = 492;
  for (const auto& detune : detunes) {
    addKnob(frame, CRect(x, 92, x + 32, 124), detune.tag, kColDcwAccent, true, detune.tooltip);
    addLabel(frame, CRect(x - 6, 124, x + 38, 136), detune.label, kColTextDim, 8, false,
             kCenterText);
    x += 52;
  }
}

void PDEditor::buildLinePanel(CFrame* frame, double x, int32 lineBase, const char* title) {
  CViewContainer* panel = new CViewContainer(CRect(x, 148, x + 488, 532));
  panel->setBackgroundColor(kColPanel);
  frame->addView(panel);

  addLabel(panel, CRect(12, 6, 120, 22), title, kColAccent, 12, true);

  // waveform selectors
  addLabel(panel, CRect(140, 9, 196, 21), "WAVE 1st", kColTextDim, 9);
  addMenu(panel, CRect(196, 4, 306, 24), lineBase + kLineParamWaveformFirst, kWaveformEntries);
  addLabel(panel, CRect(318, 9, 348, 21), "2nd", kColTextDim, 9);
  std::vector<std::string> secondEntries = {"Off"};
  secondEntries.insert(secondEntries.end(), kWaveformEntries.begin(), kWaveformEntries.end());
  addMenu(panel, CRect(348, 4, 458, 24), lineBase + kLineParamWaveformSecond, secondEntries);

  // sustain/end option lists shared by the three EG strips
  std::vector<std::string> sustainEntries = {"Off"};
  for (int i = 1; i < kNumEgSustainPointOptions; i++) {
    sustainEntries.push_back(std::to_string(i));
  }
  std::vector<std::string> endEntries;
  for (int i = 0; i < kNumEgEndPointOptions; i++) {
    endEntries.push_back(std::to_string(i + 2));
  }

  for (int32 egIndex = 0; egIndex < 3; egIndex++) {
    double top = 34 + egIndex * 116;
    int32 egBase = lineBase + kLineParamEgBegin + egIndex * kLineParamEgBlockSize;
    const CColor& accent = kEgAccents[egIndex];
    char tooltip[64];

    addLabel(panel, CRect(12, top, 340, top + 14), kEgTitles[egIndex], accent, 9, true);

    addLabel(panel, CRect(12, top + 30, 48, top + 42), "RATE", kColTextDim, 8);
    for (int i = 0; i < kNumEgRateParams; i++) {
      snprintf(tooltip, sizeof(tooltip), "Rate %d", i + 1);
      double kx = 56 + i * 38;
      addKnob(panel, CRect(kx, top + 18, kx + 30, top + 48), egBase + kEgParamRate0 + i,
              accent, false, tooltip);
    }

    addLabel(panel, CRect(12, top + 70, 48, top + 82), "LEVEL", kColTextDim, 8);
    for (int i = 0; i < kNumEgLevelParams; i++) {
      snprintf(tooltip, sizeof(tooltip), "Level %d", i + 1);
      double kx = 56 + i * 38;
      addKnob(panel, CRect(kx, top + 58, kx + 30, top + 88), egBase + kEgParamLevel0 + i,
              accent, false, tooltip);
    }

    addLabel(panel, CRect(374, top + 18, 404, top + 30), "SUS", kColTextDim, 8);
    addMenu(panel, CRect(404, top + 16, 462, top + 34), egBase + kEgParamSustainPoint,
            sustainEntries);
    addLabel(panel, CRect(374, top + 58, 404, top + 70), "END", kColTextDim, 8);
    addMenu(panel, CRect(404, top + 56, 462, top + 74), egBase + kEgParamEndPoint, endEntries);
  }
}

bool PLUGIN_API PDEditor::open(void* parent, const PlatformType& platformType) {
  if (frame != nullptr) {
    return false;
  }

  frame = new CFrame(CRect(0, 0, kEditorWidth, kEditorHeight), this);
  frame->setBackgroundColor(kColBg);

  buildHeader(frame);
  buildGlobalRow(frame);
  buildLinePanel(frame, 8, kParamLine1Begin, "LINE 1");
  buildLinePanel(frame, 504, kParamLine2Begin, "LINE 2");

  // reflect the current parameter values before showing
  for (const auto& [tag, binding] : bindings_) {
    updateControl(tag, getController()->getParamNormalized(tag));
  }

  if (!frame->open(parent, platformType)) {
    frame->forget();
    frame = nullptr;
    return false;
  }

  static_cast<PDController*>(getController())->setActiveEditor(this);
  return true;
}

void PLUGIN_API PDEditor::close() {
  static_cast<PDController*>(getController())->setActiveEditor(nullptr);
  bindings_.clear();
  if (frame != nullptr) {
    frame->close();  // closes the platform window and forgets the frame
    frame = nullptr;
  }
}

void PDEditor::valueChanged(CControl* control) {
  auto it = bindings_.find(control->getTag());
  if (it == bindings_.end()) {
    return;
  }

  const Binding& binding = it->second;
  ParamValue normalized = binding.numOptions > 1
      ? control->getValue() / (binding.numOptions - 1)
      : control->getValueNormalized();
  getController()->setParamNormalized(control->getTag(), normalized);
  getController()->performEdit(control->getTag(), normalized);
}

void PDEditor::controlBeginEdit(CControl* control) {
  getController()->beginEdit(control->getTag());
}

void PDEditor::controlEndEdit(CControl* control) {
  getController()->endEdit(control->getTag());
}

void PDEditor::updateControl(ParamID tag, ParamValue value) {
  auto it = bindings_.find(tag);
  if (it == bindings_.end()) {
    return;
  }

  const Binding& binding = it->second;
  if (binding.numOptions > 1) {
    binding.control->setValue(
      static_cast<float>(decodeOptionIndex(value, binding.numOptions))
    );
  } else {
    binding.control->setValueNormalized(static_cast<float>(value));
  }
  binding.control->invalid();
}

}  // namespace Vst
}  // namespace Steinberg
