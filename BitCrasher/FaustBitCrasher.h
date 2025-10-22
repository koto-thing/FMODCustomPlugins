/* ------------------------------------------------------------
name: "BitCrasher"
Code generated with Faust 2.81.10 (https://faust.grame.fr)
Compilation options: -lang cpp -ct 1 -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 0
------------------------------------------------------------ */

#ifndef  __mydsp_H__
#define  __mydsp_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif

# include <algorithm>
# include <cmath>
# include <cstdint>
# include <faust/dsp/dsp.h>
# include <faust/gui/GUI.h>
# include <faust/misc.h>

#ifndef FAUSTCLASS
#define FAUSTCLASS mydsp
#endif

#ifdef __APPLE__
#define exp10f __exp10f
#define exp10 __exp10
#endif

#if defined(_WIN32)
#define RESTRICT __restrict
#else
#define RESTRICT __restrict__
#endif

class mydsp : public dsp {

public:
 FAUSTFLOAT fHslider0;
 int iRec1[2];
 float fRec0[2];
 FAUSTFLOAT fHslider1;
 int fSampleRate;

 mydsp() { }

 void metadata(Meta* m) override {
  m->declare("basics.lib/name", "Faust Basic Element Library");
  m->declare("basics.lib/sAndH:author", "Romain Michon");
  m->declare("basics.lib/version", "1.22.0");
  m->declare("compile_options", "-lang cpp -ct 1 -es 1 -mcd 16 -mdd 1024 -mdy 33 -single -ftz 0");
  m->declare("filename", "FaustBitCrasher.dsp");
  m->declare("name", "BitCrasher");
 }

 int getNumInputs() override {
  return 1;
 }
 int getNumOutputs() override {
  return 1;
 }

 static void classInit(int sample_rate) {
 }

 void instanceConstants(int sample_rate) override {
  fSampleRate = sample_rate;
 }

 void instanceResetUserInterface() override {
  fHslider0 = static_cast<FAUSTFLOAT>(4.0f);
  fHslider1 = static_cast<FAUSTFLOAT>(8.0f);
 }

 void instanceClear() override {
  for (int l0 = 0; l0 < 2; l0 = l0 + 1) {
   iRec1[l0] = 0;
  }
  for (int l1 = 0; l1 < 2; l1 = l1 + 1) {
   fRec0[l1] = 0.0f;
  }
 }

 void init(int sample_rate) override {
  classInit(sample_rate);
  instanceInit(sample_rate);
 }

 void instanceInit(int sample_rate) override {
  instanceConstants(sample_rate);
  instanceResetUserInterface();
  instanceClear();
 }

 mydsp* clone() override {
  return new mydsp();
 }

 int getSampleRate() override {
  return fSampleRate;
 }

 void buildUserInterface(UI* ui_interface) override {
  ui_interface->openVerticalBox("BitCrasher");
  ui_interface->addHorizontalSlider("bits", &fHslider1, FAUSTFLOAT(8.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(16.0f), FAUSTFLOAT(1.0f));
  ui_interface->addHorizontalSlider("downsampling", &fHslider0, FAUSTFLOAT(4.0f), FAUSTFLOAT(1.0f), FAUSTFLOAT(32.0f), FAUSTFLOAT(1.0f));
  ui_interface->closeBox();
 }

 void compute(int count, FAUSTFLOAT** RESTRICT inputs, FAUSTFLOAT** RESTRICT outputs) override {
  FAUSTFLOAT* input0 = inputs[0];
  FAUSTFLOAT* output0 = outputs[0];
  int iSlow0 = static_cast<int>(static_cast<float>(fHslider0));
  int iSlow1 = static_cast<int>(std::pow(static_cast<float>(2), static_cast<float>(static_cast<int>(static_cast<float>(fHslider1)))));
  float fSlow2 = static_cast<float>(iSlow1 + -1);
  float fSlow3 = 1.0f / (static_cast<float>(iSlow1) + -1.0f);
  for (int i0 = 0; i0 < count; i0 = i0 + 1) {
   iRec1[0] = iRec1[1] + 1;
   fRec0[0] = (((iRec1[0] % iSlow0) == 0) ? static_cast<float>(input0[i0]) : fRec0[1]);
   output0[i0] = static_cast<FAUSTFLOAT>(fSlow3 * static_cast<float>(static_cast<int>(fSlow2 * fRec0[0])));
   iRec1[1] = iRec1[0];
   fRec0[1] = fRec0[0];
  }
 }

};

#endif
