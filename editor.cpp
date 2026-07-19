#include "editor.h"

#include <cmath>
#include <cstdio>

#include "vstgui/lib/cfileselector.h"
#include "vstgui/lib/cframe.h"
#include "vstgui/lib/cgradient.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/controls/cbuttons.h"
#include "vstgui/lib/controls/cknob.h"
#include "vstgui/lib/controls/coptionmenu.h"
#include "vstgui/lib/controls/csegmentbutton.h"
#include "vstgui/lib/controls/cslider.h"
#include "vstgui/lib/controls/ctextedit.h"
#include "vstgui/lib/controls/ctextlabel.h"
#include "controller.h"

namespace Steinberg {
namespace Vst {

using namespace VSTGUI;

namespace {

constexpr int kEditorWidth = 1048;
constexpr int kEditorHeight = 752;

// tags outside the parameter id space
constexpr int32_t kSkinMenuTag = 100000;
constexpr int32_t kPresetSaveTag = 100001;
constexpr int32_t kPresetLoadTag = 100002;

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
    // Modeled after the CZ-101 hardware: metallic grey panel with black
    // print, charcoal body, beige data keys, and a pale green LCD.
    "CZ",
    CColor(97, 99, 103),    // bg (brushed body grey)
    CColor(137, 140, 144),  // panel (metallic grey)
    CColor(227, 219, 197),  // control (beige data keys)
    CColor(84, 86, 90),     // controlFrame
    CColor(26, 27, 30),     // text (black print)
    CColor(56, 58, 62),     // textDim
    CColor(45, 45, 48),     // accent (pressed charcoal key)
    CColor(238, 232, 218),  // accentText (cream on charcoal)
    CColor(52, 53, 56),     // knobBody (charcoal)
    CColor(235, 230, 216),  // knobPointer (cream)
    CColor(163, 175, 146),  // scopeBg (LCD green)
    CColor(142, 154, 124),  // scopeGrid
    CColor(38, 54, 34),     // scopeTrace (dark LCD segments)
    {CColor(18, 112, 52), CColor(16, 88, 160), CColor(168, 38, 34)},
  },
};
constexpr int32 kNumSkins = sizeof(kSkins) / sizeof(kSkins[0]);

const char* const kEgTitles[] = {"DCO ENV", "DCW ENV", "DCA ENV"};

const std::vector<std::string> kWaveformEntries = {
  "1 Saw Tooth", "2 Square", "3 Pulse", "4 Double Sine", "5 Saw Pulse",
  "6 Resonance I", "7 Resonance II", "8 Resonance III",
};

SharedPointer<CFontDesc> makeFont(double size, bool bold) {
  return makeOwned<CFontDesc>("Consolas", size, bold ? kBoldFace : kNormalFace);
}

int clampInt(int value, int low, int high) {
  return value < low ? low : (value > high ? high : value);
}

std::string formatParamValue(double normalized, int32 signedRange, int32 displayMax) {
  char text[16];
  if (signedRange > 0) {
    int plain = decodeSignedOption(normalized, signedRange);
    if (plain == 0) {
      snprintf(text, sizeof(text), "0");
    } else {
      snprintf(text, sizeof(text), "%+d", plain);
    }
  } else {
    snprintf(text, sizeof(text), "%d", static_cast<int>(lround(normalized * displayMax)));
  }
  return text;
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
    context->setLineWidth(2.6);
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
    body.inset(6.0, 6.0);
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
    CPoint inner(center.x + dir.x * radius * 0.28, center.y + dir.y * radius * 0.28);
    CPoint outer(center.x + dir.x * radius * 0.92, center.y + dir.y * radius * 0.92);
    context->setLineStyle(CLineStyle(CLineStyle::kLineCapRound));
    context->setLineWidth(2.6);
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
  bindings_[tag] = Binding{knob, 0, nullptr, 0, 99};
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
  bindings_[tag] = Binding{slider, 0, nullptr, 0, 99};
  return slider;
}

void PDEditor::addMenu(CViewContainer* parent, const CRect& rect, ParamID tag,
                       const std::vector<std::string>& entries) {
  COptionMenu* menu = new COptionMenu(rect, this, tag);
  for (const std::string& entry : entries) {
    menu->addEntry(entry.c_str());
  }
  menu->setFont(makeFont(13, false));
  menu->setFontColor(skin().text);
  menu->setBackColor(skin().control);
  menu->setFrameColor(skin().controlFrame);
  menu->setHoriAlign(kCenterText);
  menu->setStyle(CParamDisplay::kRoundRectStyle);
  menu->setRoundRectRadius(3.0);
  parent->addView(menu);
  bindings_[tag] = Binding{menu, static_cast<int32>(entries.size()), nullptr, 0, 99};
}

void PDEditor::addSegmentButton(CViewContainer* parent, const CRect& rect, ParamID tag,
                                const std::vector<std::string>& segments) {
  CSegmentButton* button = new CSegmentButton(rect, this, tag);
  for (const std::string& name : segments) {
    CSegmentButton::Segment segment;
    segment.name = name.c_str();
    button->addSegment(std::move(segment));
  }
  button->setFont(makeFont(13, true));
  button->setTextColor(skin().text);
  button->setTextColorHighlighted(skin().accentText);
  button->setFrameColor(skin().controlFrame);
  button->setFrameWidth(1.0);
  button->setRoundRadius(4.0);
  button->setGradient(CGradient::create(0, 1, skin().control, skin().control));
  button->setGradientHighlighted(CGradient::create(0, 1, skin().accent, skin().accent));
  parent->addView(button);
  bindings_[tag] = Binding{button, 0, nullptr, 0, 99};
}

void PDEditor::addTextButton(CViewContainer* parent, const CRect& rect, int32_t tag,
                             const char* title) {
  CTextButton* button = new CTextButton(rect, this, tag, title, CTextButton::kKickStyle);
  button->setFont(makeFont(12, true));
  button->setTextColor(skin().text);
  button->setTextColorHighlighted(skin().accentText);
  button->setFrameColor(skin().controlFrame);
  button->setFrameColorHighlighted(skin().controlFrame);
  button->setRoundRadius(3.0);
  button->setGradient(CGradient::create(0, 1, skin().control, skin().control));
  button->setGradientHighlighted(CGradient::create(0, 1, skin().accent, skin().accent));
  parent->addView(button);
}

void PDEditor::attachValueLabel(CViewContainer* parent, const CRect& rect, ParamID tag,
                                int32 signedRange, int32 displayMax) {
  // a text field bound to the same tag: clicking it allows typing the value
  CTextEdit* edit = new CTextEdit(rect, this, tag, "");
  edit->setFont(makeFont(11, false));
  edit->setFontColor(skin().text);
  edit->setBackColor(kTransparentCColor);
  edit->setFrameColor(kTransparentCColor);
  edit->setHoriAlign(kCenterText);
  edit->setStyle(CParamDisplay::kNoFrame);
  edit->setTooltipText("Click to type a value");
  edit->setStringToValueFunction(
    [signedRange, displayMax](UTF8StringPtr txt, float& result, CTextEdit*) -> bool {
      if (txt == nullptr) {
        return false;
      }
      int plain = atoi(txt);
      if (signedRange > 0) {
        plain = clampInt(plain, -signedRange, signedRange);
        result = static_cast<float>(plain + signedRange) / (2 * signedRange);
      } else {
        plain = clampInt(plain, 0, displayMax);
        result = static_cast<float>(plain) / displayMax;
      }
      return true;
    }
  );
  edit->setValueToStringFunction2(
    [signedRange, displayMax](float value, std::string& result, CParamDisplay*) -> bool {
      result = formatParamValue(value, signedRange, displayMax);
      return true;
    }
  );
  parent->addView(edit);

  auto it = bindings_.find(tag);
  if (it != bindings_.end()) {
    it->second.valueLabel = edit;
    it->second.signedRange = signedRange;
    it->second.displayMax = displayMax;
  }
}

void PDEditor::refreshValueLabel(ParamID tag, ParamValue value) {
  auto it = bindings_.find(tag);
  if (it == bindings_.end() || it->second.valueLabel == nullptr) {
    return;
  }
  it->second.valueLabel->setText(
    formatParamValue(value, it->second.signedRange, it->second.displayMax).c_str()
  );
  it->second.valueLabel->invalid();
}

void PDEditor::buildHeader(CFrame* frame) {
  addLabel(frame, CRect(24, 8, 120, 56), "PD", skin().text, 36, true);
  addLabel(frame, CRect(126, 10, 470, 26), "CZ SERIES TRIBUTE", skin().accent, 11, true);
  addLabel(frame, CRect(126, 27, 470, 44), "PHASE DISTORTION SYNTHESIZER", skin().textDim, 12);

  addLabel(frame, CRect(490, 10, 610, 24), "PRESET", skin().textDim, 11, true);
  addTextButton(frame, CRect(490, 27, 548, 51), kPresetSaveTag, "SAVE");
  addTextButton(frame, CRect(556, 27, 614, 51), kPresetLoadTag, "LOAD");

  addKnob(frame, CRect(644, 8, 688, 52), kParamVolume, skin().accent, false, "Volume");
  addLabel(frame, CRect(618, 52, 714, 64), "VOLUME", skin().textDim, 10, false, kCenterText);
  attachValueLabel(frame, CRect(690, 19, 734, 36), kParamVolume, 0, 127);

  OscilloscopeView* scope = new OscilloscopeView(CRect(744, 6, 1040, 62), pdController(), skin());
  frame->addView(scope);
}

void PDEditor::buildGlobalRow(CFrame* frame) {
  addLabel(frame, CRect(24, 76, 170, 90), "LINE SELECT", skin().textDim, 11, true);
  addSegmentButton(frame, CRect(24, 94, 280, 126), kParamLineSelect,
                   {"1", "2", "1+1'", "1+2'"});
  bindings_[kParamLineSelect].control->setTooltipText(
    "Sounding line configuration (1' / 2' are detuned)"
  );

  addLabel(frame, CRect(310, 76, 420, 90), "KEY ASSIGN", skin().textDim, 11, true);
  addSegmentButton(frame, CRect(310, 94, 440, 126), kParamMonoPoly, {"POLY", "MONO"});

  addLabel(frame, CRect(478, 76, 556, 90), "DETUNE", skin().textDim, 11, true);
  const struct {
    ParamID tag;
    int32 range;
    const char* label;
    const char* tooltip;
  } detunes[] = {
    {kParamDetuneOctave, kDetuneOctaveRange, "OCT", "Detune Octave"},
    {kParamDetuneNote, kDetuneNoteRange, "NOTE", "Detune Note"},
    {kParamDetuneFine, kDetuneFineRange, "FINE", "Detune Fine"},
  };
  double x = 478;
  for (const auto& detune : detunes) {
    addKnob(frame, CRect(x, 92, x + 38, 130), detune.tag, skin().eg[1], true, detune.tooltip);
    addLabel(frame, CRect(x - 10, 131, x + 48, 144), detune.label, skin().textDim, 10, false,
             kCenterText);
    attachValueLabel(frame, CRect(x - 10, 144, x + 48, 160), detune.tag, detune.range);
    x += 62;
  }

  addLabel(frame, CRect(700, 76, 760, 90), "SKIN", skin().textDim, 11, true);
  COptionMenu* skinMenu = new COptionMenu(CRect(700, 94, 800, 118), this, kSkinMenuTag);
  for (int32 i = 0; i < kNumSkins; i++) {
    skinMenu->addEntry(kSkins[i].name);
  }
  skinMenu->setFont(makeFont(13, false));
  skinMenu->setFontColor(skin().text);
  skinMenu->setBackColor(skin().control);
  skinMenu->setFrameColor(skin().controlFrame);
  skinMenu->setHoriAlign(kCenterText);
  skinMenu->setStyle(CParamDisplay::kRoundRectStyle);
  skinMenu->setRoundRectRadius(3.0);
  skinMenu->setValue(static_cast<float>(pdController()->getSkinIndex()));
  frame->addView(skinMenu);
}

void PDEditor::buildLinePanel(CFrame* frame, double x, int32 lineBase, const char* title) {
  CViewContainer* panel = new CViewContainer(CRect(x, 166, x + 512, 747));
  panel->setBackgroundColor(skin().panel);
  frame->addView(panel);

  int lineIndex = lineBase == kParamLine1Begin ? 0 : 1;
  lineTitles_[lineIndex] = addLabel(panel, CRect(12, 4, 186, 24), title, skin().accent, 14, true);

  // waveform selectors
  addLabel(panel, CRect(190, 7, 258, 21), "WAVE 1st", skin().textDim, 11);
  addMenu(panel, CRect(258, 3, 364, 25), lineBase + kLineParamWaveformFirst, kWaveformEntries);
  addLabel(panel, CRect(372, 7, 402, 21), "2nd", skin().textDim, 11);
  std::vector<std::string> secondEntries = {"Off"};
  secondEntries.insert(secondEntries.end(), kWaveformEntries.begin(), kWaveformEntries.end());
  addMenu(panel, CRect(402, 3, 508, 25), lineBase + kLineParamWaveformSecond, secondEntries);

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
    double top = 34 + egIndex * 182;
    int32 egBase = lineBase + kLineParamEgBegin + egIndex * kLineParamEgBlockSize;
    const CColor& accent = skin().eg[egIndex];
    char tooltip[64];

    EgStrip strip{};
    strip.sustainTag = egBase + kEgParamSustainPoint;
    strip.endTag = egBase + kEgParamEndPoint;
    strip.egIndex = egIndex;

    addLabel(panel, CRect(12, top, 340, top + 15), kEgTitles[egIndex], accent, 12, true);

    // step number header row (also carries the sustain marker)
    addLabel(panel, CRect(8, top + 16, 54, top + 31), "STEP", skin().textDim, 10);
    for (int i = 0; i < kNumEgRateParams; i++) {
      double kx = 56 + i * 44;
      strip.stepLabels[i] = addLabel(panel, CRect(kx - 3, top + 16, kx + 47, top + 31),
                                     std::to_string(i + 1).c_str(), skin().textDim, 10, false,
                                     kCenterText);
    }

    // level sliders above their step's rate dial column
    addLabel(panel, CRect(8, top + 62, 54, top + 76), "LEVEL", skin().textDim, 10);
    for (int i = 0; i < kNumEgLevelParams; i++) {
      snprintf(tooltip, sizeof(tooltip), "Level %d", i + 1);
      double kx = 56 + i * 44;
      ParamID tag = egBase + kEgParamLevel0 + i;
      strip.levelSliders[i] = addLevelSlider(
        panel, CRect(kx + 13, top + 33, kx + 31, top + 103), tag, accent, tooltip
      );
      attachValueLabel(panel, CRect(kx - 3, top + 105, kx + 47, top + 120), tag);
    }

    // rate dials (bottom row)
    addLabel(panel, CRect(8, top + 135, 54, top + 149), "RATE", skin().textDim, 10);
    for (int i = 0; i < kNumEgRateParams; i++) {
      snprintf(tooltip, sizeof(tooltip), "Rate %d", i + 1);
      double kx = 56 + i * 44;
      ParamID tag = egBase + kEgParamRate0 + i;
      strip.rateKnobs[i] = addKnob(panel, CRect(kx + 5, top + 124, kx + 39, top + 158), tag,
                                   accent, false, tooltip);
      attachValueLabel(panel, CRect(kx - 3, top + 160, kx + 47, top + 175), tag);
    }

    addLabel(panel, CRect(410, top + 36, 448, top + 50), "SUS", skin().textDim, 10);
    addMenu(panel, CRect(448, top + 33, 500, top + 53), strip.sustainTag, sustainEntries);
    addLabel(panel, CRect(410, top + 74, 448, top + 88), "END", skin().textDim, 10);
    addMenu(panel, CRect(448, top + 71, 500, top + 91), strip.endTag, endEntries);

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
  buildLinePanel(frame, 528, kParamLine2Begin, "LINE 2");
}

void PDEditor::restyleLineTitles() {
  LineSelect mode = static_cast<LineSelect>(decodeOptionIndex(
    getController()->getParamNormalized(kParamLineSelect),
    static_cast<int>(LineSelect::kNumLineSelects)
  ));
  bool audible[2] = {
    mode != LineSelect::kLine2,
    mode == LineSelect::kLine2 || mode == LineSelect::kLine1Plus2Detuned,
  };
  const char* const names[2] = {"LINE 1", "LINE 2"};
  for (int i = 0; i < 2; i++) {
    if (lineTitles_[i] == nullptr) {
      continue;
    }
    lineTitles_[i]->setText(audible[i] ? names[i]
                                       : (std::string(names[i]) + "  (MUTED)").c_str());
    lineTitles_[i]->setFontColor(audible[i] ? skin().accent : skin().textDim);
    lineTitles_[i]->invalid();
  }
}

void PDEditor::syncAllControls() {
  for (const auto& [tag, binding] : bindings_) {
    updateControl(tag, getController()->getParamNormalized(tag));
  }
  for (const EgStrip& strip : strips_) {
    restyleStrip(strip);
  }
  restyleLineTitles();
}

void PDEditor::rebuildUi() {
  if (frame == nullptr) {
    return;
  }
  frame->removeAll();
  bindings_.clear();
  strips_.clear();
  stripByStyleTag_.clear();
  lineTitles_ = {};
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
  lineTitles_ = {};
  if (frame != nullptr) {
    frame->close();  // closes the platform window and forgets the frame
    frame = nullptr;
  }
}

void PDEditor::onSavePreset() {
  CNewFileSelector* selector = CNewFileSelector::create(frame, CNewFileSelector::kSelectSaveFile);
  if (selector == nullptr) {
    return;
  }
  selector->setTitle("Save Preset");
  selector->setDefaultExtension(CFileExtension("VST3 Preset", "vstpreset"));
  PDController* controller = pdController();
  selector->run([controller](CNewFileSelector* sel) {
    if (sel->getNumSelectedFiles() > 0) {
      controller->savePresetFile(sel->getSelectedFile(0));
    }
  });
  selector->forget();
}

void PDEditor::onLoadPreset() {
  CNewFileSelector* selector = CNewFileSelector::create(frame, CNewFileSelector::kSelectFile);
  if (selector == nullptr) {
    return;
  }
  selector->setTitle("Load Preset");
  selector->setDefaultExtension(CFileExtension("VST3 Preset", "vstpreset"));
  PDController* controller = pdController();
  selector->run([controller](CNewFileSelector* sel) {
    if (sel->getNumSelectedFiles() > 0) {
      controller->loadPresetFile(sel->getSelectedFile(0));
    }
  });
  selector->forget();
}

void PDEditor::valueChanged(CControl* control) {
  int32_t tag = control->getTag();
  if (tag == kSkinMenuTag) {
    pdController()->setSkinIndex(static_cast<int32>(control->getValue()));
    rebuildUi();
    return;
  }
  if (tag == kPresetSaveTag) {
    if (control->getValue() > 0.5f) {
      onSavePreset();
    }
    return;
  }
  if (tag == kPresetLoadTag) {
    if (control->getValue() > 0.5f) {
      onLoadPreset();
    }
    return;
  }

  auto it = bindings_.find(tag);
  if (it == bindings_.end()) {
    return;
  }

  const Binding& binding = it->second;
  ParamValue normalized = binding.numOptions > 1
      ? control->getValue() / (binding.numOptions - 1)
      : control->getValueNormalized();
  getController()->setParamNormalized(tag, normalized);
  getController()->performEdit(tag, normalized);
}

void PDEditor::controlBeginEdit(CControl* control) {
  if (control->getTag() >= kSkinMenuTag) {
    return;
  }
  getController()->beginEdit(control->getTag());
}

void PDEditor::controlEndEdit(CControl* control) {
  if (control->getTag() >= kSkinMenuTag) {
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

  auto setEnabled = [this](CControl* control, bool enabled) {
    control->setAlphaValue(enabled ? 1.0f : 0.25f);
    control->setMouseEnabled(enabled);
    control->invalid();
    auto it = bindings_.find(control->getTag());
    if (it != bindings_.end() && it->second.valueLabel != nullptr) {
      it->second.valueLabel->setAlphaValue(enabled ? 1.0f : 0.25f);
      it->second.valueLabel->setMouseEnabled(enabled);
      it->second.valueLabel->invalid();
    }
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
    label->setFont(makeFont(10, isSustain));
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
  refreshValueLabel(tag, value);

  auto stripIt = stripByStyleTag_.find(tag);
  if (stripIt != stripByStyleTag_.end()) {
    restyleStrip(strips_[stripIt->second]);
  }
  if (tag == kParamLineSelect) {
    restyleLineTitles();
  }
}

}  // namespace Vst
}  // namespace Steinberg
