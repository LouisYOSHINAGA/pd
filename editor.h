#pragma once

#include <array>
#include <map>
#include <string>
#include <vector>
#include "public.sdk/source/vst/vstguieditor.h"
#include "vstgui/lib/cvstguitimer.h"
#include "const.h"

namespace Steinberg {
namespace Vst {

class PDController;

// Color palette of one selectable skin.
struct PDSkin {
  const char* name;
  VSTGUI::CColor bg;
  VSTGUI::CColor panel;
  VSTGUI::CColor control;       // menu / segment body
  VSTGUI::CColor controlFrame;  // outlines, knob track rings
  VSTGUI::CColor text;
  VSTGUI::CColor textDim;
  VSTGUI::CColor accent;
  VSTGUI::CColor accentText;    // text on accent-filled surfaces
  VSTGUI::CColor knobBody;
  VSTGUI::CColor knobPointer;
  VSTGUI::CColor scopeBg;
  VSTGUI::CColor scopeGrid;
  VSTGUI::CColor scopeTrace;
  VSTGUI::CColor eg[3];         // DCO (green), DCW (blue), DCA (red)
};

// KORG-style waveform monitor: renders the latest output frame received from
// the processor, auto-scaled in amplitude and stabilized with a rising-edge
// trigger.
class OscilloscopeView : public VSTGUI::CView {
 public:
  OscilloscopeView(const VSTGUI::CRect& size, PDController* controller, const PDSkin& skin);
  void draw(VSTGUI::CDrawContext* context) override;
  bool attached(VSTGUI::CView* parent) override;
  bool removed(VSTGUI::CView* parent) override;

 private:
  PDController* controller_;
  PDSkin skin_;
  VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> timer_;
  std::vector<float> frame_;
};

// Programmatically built editor: header with title/volume/oscilloscope, a
// global row (line select, key assign, detune, CC edit line, skin), and one
// panel per line with waveform selectors and the three EG strips (level
// sliders on top, rate dials below, with sustain/end visualization).
class PDEditor : public VSTGUIEditor, public VSTGUI::IControlListener {
 public:
  PDEditor(void* controller);

  bool PLUGIN_API open(void* parent, const VSTGUI::PlatformType& platformType) override;
  void PLUGIN_API close() override;

  // IControlListener
  void valueChanged(VSTGUI::CControl* control) override;
  void controlBeginEdit(VSTGUI::CControl* control) override;
  void controlEndEdit(VSTGUI::CControl* control) override;

  // Reflects a parameter change (from automation or another editor) into the
  // bound control.
  void updateControl(ParamID tag, ParamValue value);

 private:
  // numOptions > 1 marks an option-menu control whose CControl value is the
  // raw item index; 0 marks a control operating on normalized values.
  struct Binding {
    VSTGUI::CControl* control;
    int32 numOptions;
  };

  // Views of one EG strip, kept for sustain/end visualization.
  struct EgStrip {
    std::array<VSTGUI::CControl*, kNumEgRateParams> rateKnobs;
    std::array<VSTGUI::CControl*, kNumEgLevelParams> levelSliders;
    std::array<VSTGUI::CTextLabel*, kNumEgRateParams> stepLabels;
    ParamID sustainTag;
    ParamID endTag;
    int egIndex;  // 0=DCO, 1=DCW, 2=DCA
  };

  const PDSkin& skin() const;
  PDController* pdController() const;

  VSTGUI::CTextLabel* addLabel(VSTGUI::CViewContainer* parent, const VSTGUI::CRect& rect,
                               const char* text, const VSTGUI::CColor& color, double fontSize,
                               bool bold = false,
                               VSTGUI::CHoriTxtAlign align = VSTGUI::kLeftText);
  VSTGUI::CControl* addKnob(VSTGUI::CViewContainer* parent, const VSTGUI::CRect& rect,
                            ParamID tag, const VSTGUI::CColor& coronaColor, bool bipolar,
                            const char* tooltip);
  VSTGUI::CControl* addLevelSlider(VSTGUI::CViewContainer* parent, const VSTGUI::CRect& rect,
                                   ParamID tag, const VSTGUI::CColor& accent,
                                   const char* tooltip);
  void addMenu(VSTGUI::CViewContainer* parent, const VSTGUI::CRect& rect, ParamID tag,
               const std::vector<std::string>& entries);
  void addSegmentButton(VSTGUI::CViewContainer* parent, const VSTGUI::CRect& rect, ParamID tag,
                        const std::vector<std::string>& segments);
  void buildHeader(VSTGUI::CFrame* frame);
  void buildGlobalRow(VSTGUI::CFrame* frame);
  void buildLinePanel(VSTGUI::CFrame* frame, double x, int32 lineBase, const char* title);
  void buildUi();
  void rebuildUi();
  void syncAllControls();

  // Applies the sustain marker and dims/disables the steps beyond the end
  // point of one EG strip.
  void restyleStrip(const EgStrip& strip);

  std::map<ParamID, Binding> bindings_;
  std::vector<EgStrip> strips_;
  std::map<ParamID, size_t> stripByStyleTag_;  // sustain/end tag -> strip index
};

}  // namespace Vst
}  // namespace Steinberg
