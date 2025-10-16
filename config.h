#pragma once

#include "pluginterfaces/base/funknown.h"
#include "processor.h"
#include "controller.h"


#define MYVST_VENDOR "Nareshika Works"
#define MYVST_URL "https://www.xxx.com/"
#define MYVST_EMAIL "mailto:9dutat.e@gmail.com"
#define MYVST_VSTNAME "PD"
#define MYVST_VERSION  "0"
#define MYVST_SUBCATEGORIES Vst::PlugType::kInstrumentSynth


namespace Steinberg{
namespace Vst {


static const FUID ProcessorUID(0x509AE5BE, 0xEA2F4ADF, 0x8C6CB8AE, 0xC306E47B);
static const FUID ControllerUID(0x2D9C9FA0, 0x08FD4C21, 0xA9F3EB5A, 0x9F7D1D5B);


} }