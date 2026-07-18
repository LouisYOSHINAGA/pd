#pragma once

#include <map>
#include <vector>
#include "public.sdk/source/vst/vstguieditor.h"
#include "vstgui/lib/cvstguitimer.h"
#include "const.h"

namespace Steinberg {
namespace Vst {

class PDController;

// KORG-style waveform monitor: renders the latest output frame received from
// the processor, with a rising-edge trigger for a stable image.
class OscilloscopeView : public VSTGUI::CView {
 public:
  OscilloscopeView(const VSTGUI::CRect& size, PDController* controller);
  void draw(VSTGUI::CDrawContext* context) override;
  bool attached(VSTGUI::CView* parent) override;
  bool removed(VSTGUI::CView* parent) override;

 private:
  PDController* controller_;
  VSTGUI::SharedPointer<VSTGUI::CVSTGUITimer> timer_;
  std::vector<float> frame_;
};

// Programmatically built editor: header with title/volume/oscilloscope, a
// global row (line select, mono/poly, detune), and one panel per line with
// waveform selectors and the three EG strips.
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

  void addLabel(VSTGUI::CViewContainer* parent, const VSTGUI::CRect& rect, const char* text,
                const VSTGUI::CColor& color, double fontSize, bool bold = false,
                VSTGUI::CHoriTxtAlign align = VSTGUI::kLeftText);
  void addKnob(VSTGUI::CViewContainer* parent, const VSTGUI::CRect& rect, ParamID tag,
               const VSTGUI::CColor& coronaColor, bool bipolar, const char* tooltip);
  void addMenu(VSTGUI::CViewContainer* parent, const VSTGUI::CRect& rect, ParamID tag,
               const std::vector<std::string>& entries);
  void addSegmentButton(VSTGUI::CViewContainer* parent, const VSTGUI::CRect& rect, ParamID tag,
                        const std::vector<std::string>& segments);
  void buildHeader(VSTGUI::CFrame* frame);
  void buildGlobalRow(VSTGUI::CFrame* frame);
  void buildLinePanel(VSTGUI::CFrame* frame, double x, int32 lineBase, const char* title);

  std::map<ParamID, Binding> bindings_;
};

}  // namespace Vst
}  // namespace Steinberg
