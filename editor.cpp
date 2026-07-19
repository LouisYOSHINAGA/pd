#include "editor.h"

#include <cmath>
#include <cstdio>

#include "vstgui/lib/cframe.h"
#include "vstgui/lib/cgradient.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/controls/cknob.h"
#include "vstgui/lib/controls/coptionmenu.h"
#include "vstgui/lib/controls/csegmentbutton.h"
#include "vstgui/lib/controls/cslider.h"
#include "vstgui/lib/controls/ctextlabel.h"
#include "controller.h"

namespace Steinberg {
namespace Vst {

using namespace VSTGUI;

namespace {

constexpr int kEditorWidth = 1000;
constexpr int kEditorHeight = 640;

// tag of the skin selector, outside the parameter id space
constexpr int32_t kSkinMenuTag = 100000;

const PDSkin kSkins[] = {
  {
    "Dark",
    CColor(16, 17, 21),     // bg
    CColor(25, 27, 33),     // panel
    CColor(35, 38, 46),     // control
    CColor(58, 62, 74),     // controlFrame
    CColor(210, 213, 219),  // text
    CColor(122, 127, 138),  // textDim
    CColor(255, 122, 44),   // accent
    CColor(24, 20, 16),     // accentText
    CColor(44, 47, 56),     // knobBody
    CColor(228, 230, 235),  // knobPointer
    CColor(8, 10, 12),      // scopeBg
    CColor(26, 32, 36),     // scopeGrid
    CColor(64, 232, 130),   // scopeTrace
    {CColor(96, 205, 128), CColor(84, 178, 234), CColor(240, 100, 92)},  // DCO/DCW/DCA
  },
  {
    "Platinum",
    CColor(222, 224, 228),  // bg
    CColor(238, 239, 242),  // panel
    CColor(250, 250, 252),  // control
    CColor(178, 182, 190),  // controlFrame
    CColor(42, 46, 54),     // text
    CColor(122, 128, 138),  // textDim
    CColor(226, 88, 22),    // accent
    CColor(252, 250, 248),  // accentText
    CColor(206, 209, 215),  // knobBody
    CColor(50, 54, 62),     // knobPointer
    CColor(22, 26, 28),     // scopeBg (hardware-display dark, on purpose)
    CColor(44, 52, 56),     // scopeGrid
    CColor(80, 235, 140),   // scopeTrace
    {CColor(28, 152, 74), CColor(24, 128, 200), CColor(206, 58, 50)},
  },
  {
    "CZ",
    CColor(40, 38, 36),     // bg (warm charcoal, CZ-1000 panel)
    CColor(52, 49, 46),     // panel
    CColor(66, 62, 58),     // control
    CColor(96, 90, 84),     // controlFrame
    CColor(238, 232, 220),  // text (cream)
    CColor(158, 150, 140),  // textDim
    CColor(226, 62, 46),    // accent (CZ data-key red)
    CColor(248, 244, 236),  // accentText
    CColor(32, 30, 28),     // knobBody
    CColor(240, 234, 220),  // knobPointer
    CColor(14, 13, 12),     // scopeBg
    CColor(42, 38, 32),     // scopeGrid
    CColor(255, 190, 74),   // scopeTrace (amber display)
    {CColor(122, 200, 112), CColor(114, 188, 220), CColor(238, 116, 92)},
  },
};
constexpr int32 kNumSkins = sizeof(kSkins) / sizeof(kSkins[0]);

const char* const kEgTitles[] = {"DCO ENVELOPE / PITCH", "DCW ENVELOPE / TIMBRE",
                                 "DCA ENVELOPE / AMP"};

const std::vector<std::string> kWaveformEntries = {
  "1 Saw Tooth", "2 Square", "3 Pulse", "4 Double Sine", "5 Saw Pulse",
  "6 Resonance I", "7 Resonance II", "8 Resonance III",
};

SharedPointer<CFontDesc> makeFont(double size, bool bold) {
  return makeOwned<CFontDesc>("Segoe UI", size, bold ? kBoldFace : kNormalFace);
}

// Flat knob with a solid body, pointer line and value arc: compact but with
// the presence of a real dial. Inherits CKnob's mouse handling only.
class PDKnob : public CKnob {
 public:
  PDKnob(const CRect& size, IControlListener* listener, int32_t tag, const PDSkin& skin,
         const CColor& accent, bool bipolar)
      : CKnob(size, listener, tag, nullptr, nullptr),
        body_(skin.knobBody),
        track_(skin.controlFrame),
        pointer_(skin.knobPointer),
        accent_(accent),
        bipolar_(bipolar) {
  }

  void draw(CDrawContext* context) override {
    static constexpr double kStartDeg = 135.0;  // down-left, sweeping 270deg clockwise
    static constexpr double kSweepDeg = 270.0;

    const CRect r = getViewSize();
    const CCoord side = std::min(r.getWidth(), r.getHeight());
    CRect square(0, 0, side, side);
    square.offset(r.left + (r.getWidth() - side) / 2, r.top + (r.getHeight() - side) / 2);

    context->setDrawMode(kAntiAliasing);

    // value arc track (outer ring)
    CRect arcRect = square;
    arcRect.inset(1.5, 1.5);
    context->setLineWidth(2.4);
    context->setFrameColor(track_);
    if (CGraphicsPath* track = context->createGraphicsPath()) {
      track->addArc(arcRect, kStartDeg, kStartDeg + kSweepDeg, true);
      context->drawGraphicsPath(track, CDrawContext::kPathStroked);
      track->forget();
    }

    // value arc
    double value = getValueNormalized();
    double from = bipolar_ ? kStartDeg + kSweepDeg / 2 : kStartDeg;
    double to = kStartDeg + value * kSweepDeg;
    context->setFrameColor(accent_);
    if (CGraphicsPath* arc = context->createGraphicsPath()) {
      arc->addArc(arcRect, from, to, to >= from);
      context->drawGraphicsPath(arc, CDrawContext::kPathStroked);
      arc->forget();
    }

    // body with rim
    CRect body = square;
    body.inset(5.5, 5.5);
    context->setFillColor(body_);
    context->drawEllipse(body, kDrawFilled);
    context->setLineWidth(1.0);
    context->setFrameColor(track_);
    context->drawEllipse(body, kDrawStroked);

    // pointer
    constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
    double angle = (kStartDeg + value * kSweepDeg) * kDegToRad;
    CPoint center(square.left + side / 2, square.top + side / 2);
    double radius = body.getWidth() / 2;
    CPoint dir(cos(angle), sin(angle));
    CPoint inner(center.x + dir.x * radius * 0.30, center.y + dir.y * radius * 0.30);
    CPoint outer(center.x + dir.x * radius * 0.92, center.y + dir.y * radius * 0.92);
    context->setLineStyle(CLineStyle(CLineStyle::kLineCapRound));
    context->setLineWidth(2.4);
    context->setFrameColor(pointer_);
    context->drawLine(std::make_pair(inner, outer));

    setDirty(false);
  }

  CLASS_METHODS(PDKnob, CKnob)

 private:
  CColor body_;
  CColor track_;
  CColor pointer_;
  CColor accent_;
  bool bipolar_;
};

}  // namespace

// ---------------------------------------------------------------------------
// OscilloscopeView

OscilloscopeView::OscilloscopeView(const CRect& size, PDController* controller,
                                   const PDSkin& skin)
    : CView(size), controller_(controller), skin_(skin) {
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

  context->setFillColor(skin_.scopeBg);
  context->drawRect(r, kDrawFilled);

  // grid: center line + quarter divisions
  context->setFrameColor(skin_.scopeGrid);
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

  // amplitude auto-scaling: the display normalizes the waveform so its shape
  // stays readable regardless of the output level
  float peak = 0.0f;
  for (int i = 0; i < window; i++) {
    peak = std::max(peak, std::abs(frame_[trigger + i]));
  }
  double gain = 0.9 / std::max(peak, 0.01f);

  auto pointAt = [&](int i) {
    double t = static_cast<double>(i) / (window - 1);
    double v = frame_[trigger + i] * gain;
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
    CColor glow = skin_.scopeTrace;
    glow.alpha = 70;
    context->setFrameColor(glow);
    context->setLineWidth(3.4);
    context->drawGraphicsPath(path, CDrawContext::kPathStroked);
    context->setFrameColor(skin_.scopeTrace);
    context->setLineWidth(1.4);
    context->drawGraphicsPath(path, CDrawContext::kPathStroked);
    path->forget();
  }

  context->setFrameColor(skin_.scopeGrid);
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

PDController* PDEditor::pdController() const {
  return static_cast<PDController*>(getController());
}

const PDSkin& PDEditor::skin() const {
  int32 index = pdController()->getSkinIndex();
  if (index < 0 || kNumSkins <= index) {
    index = 0;
  }
  return kSkins[index];
}

CTextLabel* PDEditor::addLabel(CViewContainer* parent, const CRect& rect, const char* text,
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
  return label;
}

CControl* PDEditor::addKnob(CViewContainer* parent, const CRect& rect, ParamID tag,
                            const CColor& coronaColor, bool bipolar, const char* tooltip) {
  PDKnob* knob = new PDKnob(rect, this, tag, skin(), coronaColor, bipolar);
  knob->setTooltipText(tooltip);
  parent->addView(knob);
  bindings_[tag] = Binding{knob, 0};
  return knob;
}

CControl* PDEditor::addLevelSlider(CViewContainer* parent, const CRect& rect, ParamID tag,
                                   const CColor& accent, const char* tooltip) {
  CSlider* slider = new CSlider(rect, this, tag,
                                static_cast<int32_t>(rect.top) + 1,
                                static_cast<int32_t>(rect.bottom) - 1,
                                nullptr, nullptr, CPoint(0, 0),
                                CSlider::kBottom | CSlider::kVertical);
  slider->setDrawStyle(CSlider::kDrawFrame | CSlider::kDrawBack | CSlider::kDrawValue);
  slider->setFrameColor(skin().controlFrame);
  slider->setBackColor(skin().control);
  slider->setValueColor(accent);
  slider->setFrameWidth(1.0);
  slider->setTooltipText(tooltip);
  parent->addView(slider);
  bindings_[tag] = Binding{slider, 0};
  return slider;
}

void PDEditor::addMenu(CViewContainer* parent, const CRect& rect, ParamID tag,
                       const std::vector<std::string>& entries) {
  COptionMenu* menu = new COptionMenu(rect, this, tag);
  for (const std::string& entry : entries) {
    menu->addEntry(entry.c_str());
  }
  menu->setFont(makeFont(11, false));
  menu->setFontColor(skin().text);
  menu->setBackColor(skin().control);
  menu->setFrameColor(skin().controlFrame);
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
  button->setTextColor(skin().textDim);
  button->setTextColorHighlighted(skin().accentText);
  button->setFrameColor(skin().controlFrame);
  button->setFrameWidth(1.0);
  button->setRoundRadius(4.0);
  button->setGradient(CGradient::create(0, 1, skin().control, skin().control));
  button->setGradientHighlighted(CGradient::create(0, 1, skin().accent, skin().accent));
  parent->addView(button);
  bindings_[tag] = Binding{button, 0};
}

void PDEditor::buildHeader(CFrame* frame) {
  addLabel(frame, CRect(24, 8, 110, 54), "PD", skin().text, 34, true);
  addLabel(frame, CRect(114, 12, 460, 26), "CZ SERIES TRIBUTE", skin().accent, 9, true);
  addLabel(frame, CRect(114, 28, 460, 42), "PHASE DISTORTION SYNTHESIZER", skin().textDim, 10);

  addKnob(frame, CRect(600, 10, 640, 50), kParamVolume, skin().accent, false, "Volume");
  addLabel(frame, CRect(588, 50, 652, 62), "VOLUME", skin().textDim, 8, false, kCenterText);

  OscilloscopeView* scope = new OscilloscopeView(CRect(672, 6, 976, 60), pdController(), skin());
  frame->addView(scope);
}

void PDEditor::buildGlobalRow(CFrame* frame) {
  addLabel(frame, CRect(24, 76, 140, 88), "LINE SELECT", skin().textDim, 9, true);
  addSegmentButton(frame, CRect(24, 92, 272, 122), kParamLineSelect,
                   {"1", "2", "1+1'", "1+2'"});

  addLabel(frame, CRect(300, 76, 390, 88), "KEY ASSIGN", skin().textDim, 9, true);
  addSegmentButton(frame, CRect(300, 92, 420, 122), kParamMonoPoly, {"POLY", "MONO"});

  addLabel(frame, CRect(452, 76, 512, 88), "DETUNE", skin().textDim, 9, true);
  const struct {
    ParamID tag;
    const char* label;
    const char* tooltip;
  } detunes[] = {
    {kParamDetuneOctave, "OCT", "Detune Octave"},
    {kParamDetuneNote, "NOTE", "Detune Note"},
    {kParamDetuneFine, "FINE", "Detune Fine"},
  };
  double x = 452;
  for (const auto& detune : detunes) {
    addKnob(frame, CRect(x, 90, x + 34, 124), detune.tag, skin().eg[1], true, detune.tooltip);
    addLabel(frame, CRect(x - 6, 126, x + 40, 138), detune.label, skin().textDim, 8, false,
             kCenterText);
    x += 52;
  }

  addLabel(frame, CRect(640, 76, 740, 88), "CC EDIT", skin().textDim, 9, true);
  addSegmentButton(frame, CRect(640, 92, 762, 122), kParamCcEditLine, {"LINE 1", "LINE 2"});

  addLabel(frame, CRect(806, 76, 856, 88), "SKIN", skin().textDim, 9, true);
  COptionMenu* skinMenu = new COptionMenu(CRect(806, 92, 906, 114), this, kSkinMenuTag);
  for (int32 i = 0; i < kNumSkins; i++) {
    skinMenu->addEntry(kSkins[i].name);
  }
  skinMenu->setFont(makeFont(11, false));
  skinMenu->setFontColor(skin().text);
  skinMenu->setBackColor(skin().control);
  skinMenu->setFrameColor(skin().controlFrame);
  skinMenu->setHoriAlign(kLeftText);
  skinMenu->setStyle(CParamDisplay::kRoundRectStyle);
  skinMenu->setRoundRectRadius(3.0);
  skinMenu->setValue(static_cast<float>(pdController()->getSkinIndex()));
  frame->addView(skinMenu);
}

void PDEditor::buildLinePanel(CFrame* frame, double x, int32 lineBase, const char* title) {
  CViewContainer* panel = new CViewContainer(CRect(x, 150, x + 488, 628));
  panel->setBackgroundColor(skin().panel);
  frame->addView(panel);

  addLabel(panel, CRect(12, 5, 110, 23), title, skin().accent, 12, true);

  // waveform selectors
  addLabel(panel, CRect(140, 8, 194, 20), "WAVE 1st", skin().textDim, 9);
  addMenu(panel, CRect(194, 4, 304, 24), lineBase + kLineParamWaveformFirst, kWaveformEntries);
  addLabel(panel, CRect(316, 8, 346, 20), "2nd", skin().textDim, 9);
  std::vector<std::string> secondEntries = {"Off"};
  secondEntries.insert(secondEntries.end(), kWaveformEntries.begin(), kWaveformEntries.end());
  addMenu(panel, CRect(346, 4, 456, 24), lineBase + kLineParamWaveformSecond, secondEntries);

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
    double top = 34 + egIndex * 148;
    int32 egBase = lineBase + kLineParamEgBegin + egIndex * kLineParamEgBlockSize;
    const CColor& accent = skin().eg[egIndex];
    char tooltip[64];

    EgStrip strip{};
    strip.sustainTag = egBase + kEgParamSustainPoint;
    strip.endTag = egBase + kEgParamEndPoint;
    strip.egIndex = egIndex;

    addLabel(panel, CRect(12, top, 340, top + 13), kEgTitles[egIndex], accent, 9, true);

    // level sliders (top row) above their step's rate dial column
    addLabel(panel, CRect(10, top + 42, 52, top + 54), "LEVEL", skin().textDim, 8);
    for (int i = 0; i < kNumEgLevelParams; i++) {
      snprintf(tooltip, sizeof(tooltip), "Level %d", i + 1);
      double kx = 56 + i * 38;
      strip.levelSliders[i] = addLevelSlider(
        panel, CRect(kx + 8, top + 16, kx + 24, top + 80),
        egBase + kEgParamLevel0 + i, accent, tooltip
      );
    }

    // step numbers between sliders and dials; sustain marker appears here
    for (int i = 0; i < kNumEgRateParams; i++) {
      double kx = 56 + i * 38;
      strip.stepLabels[i] = addLabel(panel, CRect(kx, top + 82, kx + 30, top + 94),
                                     std::to_string(i + 1).c_str(), skin().textDim, 8, false,
                                     kCenterText);
    }

    // rate dials (bottom row)
    addLabel(panel, CRect(10, top + 108, 52, top + 120), "RATE", skin().textDim, 8);
    for (int i = 0; i < kNumEgRateParams; i++) {
      snprintf(tooltip, sizeof(tooltip), "Rate %d", i + 1);
      double kx = 56 + i * 38;
      strip.rateKnobs[i] = addKnob(panel, CRect(kx, top + 98, kx + 30, top + 128),
                                   egBase + kEgParamRate0 + i, accent, false, tooltip);
    }

    addLabel(panel, CRect(372, top + 20, 402, top + 32), "SUS", skin().textDim, 8);
    addMenu(panel, CRect(402, top + 16, 464, top + 34), strip.sustainTag, sustainEntries);
    addLabel(panel, CRect(372, top + 58, 402, top + 70), "END", skin().textDim, 8);
    addMenu(panel, CRect(402, top + 54, 464, top + 72), strip.endTag, endEntries);

    stripByStyleTag_[strip.sustainTag] = strips_.size();
    stripByStyleTag_[strip.endTag] = strips_.size();
    strips_.push_back(strip);
  }
}

void PDEditor::buildUi() {
  frame->setBackgroundColor(skin().bg);
  buildHeader(frame);
  buildGlobalRow(frame);
  buildLinePanel(frame, 8, kParamLine1Begin, "LINE 1");
  buildLinePanel(frame, 504, kParamLine2Begin, "LINE 2");
}

void PDEditor::syncAllControls() {
  for (const auto& [tag, binding] : bindings_) {
    updateControl(tag, getController()->getParamNormalized(tag));
  }
  for (const EgStrip& strip : strips_) {
    restyleStrip(strip);
  }
}

void PDEditor::rebuildUi() {
  if (frame == nullptr) {
    return;
  }
  frame->removeAll();
  bindings_.clear();
  strips_.clear();
  stripByStyleTag_.clear();
  buildUi();
  syncAllControls();
  frame->invalid();
}

bool PLUGIN_API PDEditor::open(void* parent, const PlatformType& platformType) {
  if (frame != nullptr) {
    return false;
  }

  frame = new CFrame(CRect(0, 0, kEditorWidth, kEditorHeight), this);
  buildUi();
  syncAllControls();

  if (!frame->open(parent, platformType)) {
    frame->forget();
    frame = nullptr;
    return false;
  }

  pdController()->setActiveEditor(this);
  return true;
}

void PLUGIN_API PDEditor::close() {
  pdController()->setActiveEditor(nullptr);
  bindings_.clear();
  strips_.clear();
  stripByStyleTag_.clear();
  if (frame != nullptr) {
    frame->close();  // closes the platform window and forgets the frame
    frame = nullptr;
  }
}

void PDEditor::valueChanged(CControl* control) {
  if (control->getTag() == kSkinMenuTag) {
    pdController()->setSkinIndex(static_cast<int32>(control->getValue()));
    rebuildUi();
    return;
  }

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
  if (control->getTag() == kSkinMenuTag) {
    return;
  }
  getController()->beginEdit(control->getTag());
}

void PDEditor::controlEndEdit(CControl* control) {
  if (control->getTag() == kSkinMenuTag) {
    return;
  }
  getController()->endEdit(control->getTag());
}

void PDEditor::restyleStrip(const EgStrip& strip) {
  int sustainOption = decodeOptionIndex(
    getController()->getParamNormalized(strip.sustainTag), kNumEgSustainPointOptions
  );
  int endOption = decodeOptionIndex(
    getController()->getParamNormalized(strip.endTag), kNumEgEndPointOptions
  );
  int endStep = endOption + 1;                              // end point runs steps 0..endStep
  int sustainStep = sustainOption == 0 ? -1 : sustainOption - 1;

  auto setEnabled = [](CControl* control, bool enabled) {
    control->setAlphaValue(enabled ? 1.0f : 0.25f);
    control->setMouseEnabled(enabled);
    control->invalid();
  };

  for (int i = 0; i < kNumEgRateParams; i++) {
    setEnabled(strip.rateKnobs[i], i <= endStep);
  }
  for (int i = 0; i < kNumEgLevelParams; i++) {
    setEnabled(strip.levelSliders[i], i < endStep);
  }
  for (int i = 0; i < kNumEgRateParams; i++) {
    CTextLabel* label = strip.stepLabels[i];
    bool isSustain = i == sustainStep;
    // "\xE2\x96\xB2" is an upward triangle marking the held (sustain) step
    label->setText(isSustain ? (std::string("\xE2\x96\xB2") + std::to_string(i + 1)).c_str()
                             : std::to_string(i + 1).c_str());
    label->setFontColor(isSustain ? skin().eg[strip.egIndex] : skin().textDim);
    label->setFont(makeFont(8, isSustain));
    label->setAlphaValue(i <= endStep ? 1.0f : 0.25f);
    label->invalid();
  }
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

  auto stripIt = stripByStyleTag_.find(tag);
  if (stripIt != stripByStyleTag_.end()) {
    restyleStrip(strips_[stripIt->second]);
  }
}

}  // namespace Vst
}  // namespace Steinberg
