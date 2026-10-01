#pragma once

#include <array>
#include <map>
#include <string>
#include <vector>

#include "public.sdk/source/vst/vstguieditor.h"
#include "vstgui/lib/cvstguitimer.h"

#include "const.h"
#include "presetlibrary.h"

namespace Steinberg {
namespace Vst {

class PDController;

// Color palette of one selectable skin.
struct PDSkin {
  const char* name;
  VSTGUI::CColor bg;
  VSTGUI::CColor panel;
  VSTGUI::CColor control;       // menu / button body
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

// One row of the preset browser list: number and name as plain text. The
// current preset is filled with the accent color and the row under the mouse
// with the control color; a press reports value 1 to the listener.
class PresetRow : public VSTGUI::CControl {
 public:
  PresetRow(const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag,
            int number, const std::string& name, const PDSkin& skin);
  void setCurrent(bool current);
  void draw(VSTGUI::CDrawContext* context) override;
  VSTGUI::CMouseEventResult onMouseDown(VSTGUI::CPoint& where,
                                        const VSTGUI::CButtonState& buttons) override;
  VSTGUI::CMouseEventResult onMouseEntered(VSTGUI::CPoint& where,
                                           const VSTGUI::CButtonState& buttons) override;
  VSTGUI::CMouseEventResult onMouseExited(VSTGUI::CPoint& where,
                                          const VSTGUI::CButtonState& buttons) override;
  VSTGUI::CBaseObject* newCopy() const override { return new PresetRow(*this); }

 private:
  static constexpr double kNameLeft = 40;  // number right-aligned before this

  std::string number_;
  std::string name_;
  PDSkin skin_;
  VSTGUI::SharedPointer<VSTGUI::CFontDesc> font_;
  bool current_ = false;
  bool hover_ = false;
};

// Programmatically built editor: header with title/preset/volume/oscilloscope,
// a global row (line select, key assign, detune, CC edit line, skin), and one
// panel per line with waveform selectors and the three EG strips (level
// sliders on top, rate dials below, with sustain/end visualization and
// numeric value readouts). The header's EDIT/BROWSE switch swaps the global
// row and the line panels for the preset browser: one tab per preset folder,
// file load/save, and a numbered list of preset names that load on a click.
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
  // Reflects a change of the controller's current preset file: its name in
  // the header and its highlight in the browser.
  void currentPresetChanged();
  // Rebuilds the browser after the preset folders were rescanned.
  void presetFoldersChanged();

 private:
  // numOptions > 1 marks an option-menu control whose CControl value is the
  // raw item index; 0 marks a control operating on normalized values.
  // valueLabel, when set, is a text-editable numeric readout showing
  // 0..displayMax, or -signedRange..+signedRange when signedRange > 0.
  struct Binding {
    VSTGUI::CControl* control = nullptr;
    int32 numOptions = 0;
    VSTGUI::CTextLabel* valueLabel = nullptr;
    int32 signedRange = 0;
    int32 displayMax = 99;
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
  // A styled segment button, not yet added to a parent so that its selection
  // can be set first (see selectSegment() in editor.cpp).
  VSTGUI::CSegmentButton* createSegmentButton(const VSTGUI::CRect& rect, int32_t tag,
                                              const std::vector<std::string>& segments);
  VSTGUI::CTextButton* addTextButton(VSTGUI::CViewContainer* parent, const VSTGUI::CRect& rect,
                                     int32_t tag, const char* title);
  // Creates the numeric readout of a bound control; the readout is a text
  // field, so clicking it allows typing the value directly.
  void attachValueLabel(VSTGUI::CViewContainer* parent, const VSTGUI::CRect& rect, ParamID tag,
                        int32 signedRange = 0, int32 displayMax = 99);
  void refreshValueLabel(ParamID tag, ParamValue value);
  // Dims the panel title of a line that is not audible under the current
  // line mode.
  void restyleLineTitles();

  void buildHeader(VSTGUI::CFrame* frame);
  void buildGlobalRow(VSTGUI::CFrame* frame);
  void buildLinePanel(VSTGUI::CFrame* frame, double x, int32 lineBase, const char* title);
  void buildPresetBrowser(VSTGUI::CFrame* frame);
  void buildUi();
  void rebuildUi();
  // Rebuilds the UI once the current event has been handled, so that the
  // control that asked for it is not destroyed while still handling it.
  void rebuildUiLater();
  void forgetViews();
  void syncAllControls();
  void onSavePreset();
  void onLoadPreset();

  // Preset browser.
  void showBrowser(bool shown);
  // The folder of the selected tab, or nullptr if there are no presets.
  const PresetFolder* shownPresetFolder();
  // (Re)creates the preset buttons of the shown folder.
  void fillPresetList();
  void onPresetClicked(size_t index);
  void onAddPresetFolder();

  // Applies the sustain marker and dims/disables the steps beyond the end
  // point of one EG strip.
  void restyleStrip(const EgStrip& strip);

  std::map<ParamID, Binding> bindings_;
  std::vector<EgStrip> strips_;
  std::map<ParamID, size_t> stripByStyleTag_;  // sustain/end tag -> strip index
  std::array<VSTGUI::CTextLabel*, 2> lineTitles_{};
  VSTGUI::CViewContainer* globalRow_ = nullptr;
  std::array<VSTGUI::CViewContainer*, 2> linePanels_{};
  VSTGUI::CViewContainer* browserPanel_ = nullptr;
  VSTGUI::CViewContainer* presetList_ = nullptr;
  std::vector<PresetRow*> presetRows_;  // of the shown folder, in file order
  VSTGUI::CTextLabel* presetNameLabel_ = nullptr;
};

}  // namespace Vst
}  // namespace Steinberg
