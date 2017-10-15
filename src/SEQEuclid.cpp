// Todo make smaller lookup table
// context menu stuff
#include <string>
#include <memory>

//Change to suit wherever you place this
//#include "Fundamental.hpp"

#include "dsp/digital.hpp"

#include "erBitData.h"

#define BG_IMAGE_FILE  assetPlugin(plugin, "res/SEQEuclid.png")
#define FONT_FILE      assetPlugin(plugin, "res/Segment7Standard.ttf")


struct SEQEuclid : Module {
  enum ParamIds {
    BPM_PARAM,
    RESET_BUTTON,
    GATE_LENGTH_PARAM,
    PROB1_PARAM,
    PROB2_PARAM,
    PROB3_PARAM,
    PROB4_PARAM,
    FILL1_PARAM,
    FILL2_PARAM,
    FILL3_PARAM,
    FILL4_PARAM,
    LENGTH1_PARAM,
    LENGTH2_PARAM,
    LENGTH3_PARAM,
    LENGTH4_PARAM,
    JOG1_BUTTON,
    JOG2_BUTTON,
    JOG3_BUTTON,
    JOG4_BUTTON,
    NUM_PARAMS
  };
  enum InputIds {
    EXT_CLOCK_INPUT,
    RESET_INPUT,
    NUM_INPUTS
  };
  enum OutputIds {
    GATE_OR_OUTPUT,
    TRIGGER_OR_OUTPUT,
    GATE1_OUTPUT,
    GATE2_OUTPUT,
    GATE3_OUTPUT,
    GATE4_OUTPUT,
    TRIGGER1_OUTPUT,
    TRIGGER2_OUTPUT,
    TRIGGER3_OUTPUT,
    TRIGGER4_OUTPUT,
    NUM_OUTPUTS
  };

  // LCG see numerical recipies and wikipedia
  // The std random engine seems inapropreate for this application
  // due to it's construction you'd need to fool with creating / destroying
  // distribution fuctions in the audio hot path, which seems unwise,
  struct Lcg {
    uint_fast32_t seed;

    explicit Lcg() {
      Lcg(738);
    }

    inline explicit Lcg(uint_fast32_t seed) : seed(seed) {
      int32();
    }

    inline uint_fast32_t int32() {
      return seed = (seed >> 1) ^ (-(signed int)(seed & 1u) & 0xD0000001u);
    }

    // float 0 - 1
    inline float flt()    { return static_cast<float>(2.32830643653869629E-10 * int32()); }
    inline uint8_t int8() { return static_cast<uint8_t>(int32()); }
    inline bool bit()     { return static_cast<bool>(int32() & (1 << 0)); }
  };



  // simple timed signal state wrangler thingie
  // replace with built-in in 032
  struct GateLatch {
  public:
    enum LatchStates {
      OFF,
      RISING,
      ON,
      FALLING
    };

  private:
    double time;
    double gateTime;
    LatchStates myState;
    const double minLength = 10.0 / 10000;

  public:
    GateLatch() {
      myState = OFF;
      time = 0.0;
      gateTime = 0.0;
    }

    // Call 1st each step
    inline void step(const double &newtime) {

      const double dt =  newtime - time;
      time = newtime;

      if (gateTime > 0.0) {
        
        gateTime -= dt;
        if (gateTime <= 0.0) {
          myState = OFF;  //Set to off vs falling
          gateTime = 0.0;
        }

      }

      if(gateTime > 0.0 || myState) {

        switch (myState) {
        case RISING:  // triggered on last frame
          if (gateTime > 0.0) {
            myState = ON;
          } else {
            myState = OFF;  // Set to Off vs falling, we're catching notes on the trailing edge
          }
          break;
        case FALLING:  // Was set to falling last frame, now we go off
          myState = OFF;
          break;
        default:
          break;
        }

      }

    }

    // Call to reset
    void reset() {
      myState = OFF;
      gateTime = 0.0;
    }

    // Call to generate new signal
    inline void trigger(const double length) {
      // Only trigger if we're not already triggered
      if (!myState) {
        gateTime = length;
        myState = RISING;
      }
    }

    inline void trigger() {
      // Only trigger if we're not already triggered
      if (!myState) {
        gateTime = minLength;
        myState = RISING;
      }
    }
    
    inline int state() {
      return myState;
    }

  };



  // For accessing the patern data fill must be < length and > 0
  // if fill is >= length just output 1's
  // if fill is 0 output nothing
  struct Bank {
    int fill;   
    int length; 
    int currentStep;
    bool coinFlip;
    bool noteOn;
    SchmittTrigger jogTrigger;
    GateLatch gate;
    Lcg rng;

    
    Bank() {
      Reset();
    }

    void Reset() {
      fill = 0;
      length = 0;
      currentStep = 0;
      coinFlip = false;
      noteOn = false;
      gate.reset();
      rng.seed = 738;
    }

    // Given the current step, fill, length members and the given probablility
    // Is the note on or off?
    // If on set the gate
    void SetNote(const float p, const float glength) {
      noteOn = false;

      if (fill > 0) {
        // Flip coin
        if (p < 0.999f) {
          if (rng.flt() <= 1.0f - p) {
            coinFlip = true;
          }
        }
        // Normal operations
        if (coinFlip == false) {

          if (fill < length) {
            const patternBucket pattern_ref(&(bit_pattern_table[((fill * (SEQUENCE_MAX + 1)) + length)]));
            if (pattern_ref[currentStep]) {
              gate.trigger(glength);
              noteOn = true;
            }
          }  else if (coinFlip == false) {  // if fill > length output a gate
            gate.trigger(glength);
            noteOn = true;
          }
        }
      }
    }

    void AdvanceStep() {
      coinFlip = false;  // Make sure we clear any old random note off events
      currentStep++;
      if (currentStep + 1 > length) {
        currentStep = 0;
      }
    }

  };



  
  bool running = true;
  SchmittTrigger clockTrigger;  // for external clock
  SchmittTrigger resetTrigger;  // reset button

  Bank bank1;
  Bank bank2;
  Bank bank3;
  Bank bank4;
    
  double time = 0.0;
  double dTime = 1.0 / static_cast<double>(gSampleRate);
  int bpm = 120;
  double timerLength = 1.0 / (static_cast<double>(bpm) / 60.0);
  double timerTime = timerLength;

  // Lights
  float gatesLight = 0.0;

  SEQEuclid() {
      params.resize(NUM_PARAMS);
      inputs.resize(NUM_INPUTS);
      outputs.resize(NUM_OUTPUTS);
  }
  
  // Called via menu
  void initialize() {
    time = 0.0;
    dTime = 1.0 / static_cast<double>(gSampleRate);
    bpm = 120;
    timerLength = 1.0 / (static_cast<double>(bpm) / 60.0);
    timerTime = timerLength;

    bank1.Reset();
    bank2.Reset();
    bank3.Reset();
    bank4.Reset();
  }
  
  //Todo
  void randomize() {
  }

  void step();
};


void SEQEuclid::step() {
  const float lightLambda = 0.075;
  bool nextStep = false;

  // Do clock stuff
  if (running) {
    time += dTime;

    if (inputs[EXT_CLOCK_INPUT].active) { 

      if (clockTrigger.process(inputs[EXT_CLOCK_INPUT].value)) {
        nextStep = true;
      }

    }  else {

      timerLength = 1.0 / (static_cast<double>(bpm) / 60.0);
      timerTime -= dTime;
      
      if (dTime > timerTime) {
        timerTime = 0.0;
      }  else {
        timerTime -= dTime;
      }

      if (timerTime <= 0.0) {
        nextStep = true;
        timerTime = timerLength;
      }

    }

    bank1.gate.step(time);
    bank2.gate.step(time);
    bank3.gate.step(time);
    bank4.gate.step(time);
  }



  // Deal with inputs and button presses

  // BPM param
  bpm = floor(params[BPM_PARAM].value);

  // Reset inputs
  if (resetTrigger.process(params[RESET_BUTTON].value + inputs[RESET_INPUT].value)) {
      bank1.currentStep = 0;
      bank1.gate.reset();
      
      bank2.currentStep = 0;
      bank2.gate.reset();
      
      bank3.currentStep = 0;
      bank3.gate.reset();
  
      bank4.currentStep = 0;
      bank4.gate.reset();
  }

  // Bank controls

  // Fill and length
  bank1.fill   = floor(params[FILL1_PARAM].value);
  bank1.length = floor(params[LENGTH1_PARAM].value);

  bank2.fill   = floor(params[FILL2_PARAM].value);
  bank2.length = floor(params[LENGTH2_PARAM].value);
  
  bank3.fill   = floor(params[FILL3_PARAM].value);
  bank3.length = floor(params[LENGTH3_PARAM].value);
  
  bank4.fill   = floor(params[FILL4_PARAM].value);
  bank4.length = floor(params[LENGTH4_PARAM].value);

  // Jog button
  if (bank1.jogTrigger.process(params[JOG1_BUTTON].value)) {
    bank1.AdvanceStep();
  }
  if (bank2.jogTrigger.process(params[JOG2_BUTTON].value)) {
    bank2.AdvanceStep();
  }
  if (bank3.jogTrigger.process(params[JOG3_BUTTON].value)) {
    bank3.AdvanceStep();
  }
  if (bank4.jogTrigger.process(params[JOG4_BUTTON].value)) {
    bank4.AdvanceStep();
  }
    
  // Advance step
  if (nextStep) {
    bank1.AdvanceStep();
    bank2.AdvanceStep();
    bank3.AdvanceStep();
    bank4.AdvanceStep();
  }

  // Generate output
    
  // See if our notes are on this step
  if (nextStep) {
    bank1.SetNote(params[PROB1_PARAM].value, 
                  timerLength * params[GATE_LENGTH_PARAM].value);
    bank2.SetNote(params[PROB2_PARAM].value, 
                  timerLength * params[GATE_LENGTH_PARAM].value);
    bank3.SetNote(params[PROB3_PARAM].value, 
                  timerLength * params[GATE_LENGTH_PARAM].value);
    bank4.SetNote(params[PROB4_PARAM].value, 
                  timerLength * params[GATE_LENGTH_PARAM].value);
  }
      
  // Set output high if there's a note currently latched on
  const float gate1 = (bank1.gate.state()) ? 10.0f : 0.0f;
  const float gate2 = (bank2.gate.state()) ? 10.0f : 0.0f;
  const float gate3 = (bank3.gate.state()) ? 10.0f : 0.0f;
  const float gate4 = (bank4.gate.state()) ? 10.0f : 0.0f;
    
  // blast out a trigger for new events
  const float trigger1 = (bank1.noteOn && nextStep) ? 10.0f : 0.0f;
  const float trigger2 = (bank2.noteOn && nextStep) ? 10.0f : 0.0f;
  const float trigger3 = (bank3.noteOn && nextStep) ? 10.0f : 0.0f;
  const float trigger4 = (bank4.noteOn && nextStep) ? 10.0f : 0.0f;
  
  // Setup summed outputs
  const float gateOr = (gate1 || gate2 || gate3 || gate4) ? 10.0f : 0.0f;
  const float triggerOr = (trigger1 || trigger2 || trigger3 || trigger4) ? 10.0f : 0.0f;
  
  // Send outputs out
  gatesLight = (gateOr >= 1.0f) ? 1.0 : 0.0;

  outputs[GATE1_OUTPUT].value = gate1;
  outputs[GATE2_OUTPUT].value = gate2;
  outputs[GATE3_OUTPUT].value = gate3;
  outputs[GATE4_OUTPUT].value = gate4;

  outputs[TRIGGER1_OUTPUT].value = trigger1;
  outputs[TRIGGER2_OUTPUT].value = trigger2;
  outputs[TRIGGER3_OUTPUT].value = trigger3;
  outputs[TRIGGER4_OUTPUT].value = trigger4;

  outputs[GATE_OR_OUTPUT].value = gateOr;
  outputs[TRIGGER_OR_OUTPUT].value = triggerOr;

}


struct SEQEuclidDisplay : TransparentWidget {
  int *value;
  std::shared_ptr<Font> font;

  SEQEuclidDisplay() {
    font = Font::load(FONT_FILE);
  }

  void draw(NVGcontext *vg) {
    // Background
    NVGcolor backgroundColor = nvgRGB(0x74, 0x44, 0x44);
    NVGcolor borderColor = nvgRGB(0x10, 0x10, 0x10);
    nvgBeginPath(vg);
    nvgRoundedRect(vg, 0.0, 0.0, box.size.x, box.size.y, 5.0);
    nvgFillColor(vg, backgroundColor);
    nvgFill(vg);
    nvgStrokeWidth(vg, 1.0);
    nvgStrokeColor(vg, borderColor);
    nvgStroke(vg);

    nvgFontSize(vg, 36);
    nvgFontFaceId(vg, font->handle);
    nvgTextLetterSpacing(vg, 2.5);

    std::string to_display = std::to_string(*value);
    Vec textPos = Vec(7.0f, 35.0f);

    NVGcolor textColor = nvgRGB(0xdf, 0xd2, 0x2c);
    nvgFillColor(vg, nvgTransRGBA(textColor, 16));
    nvgText(vg, textPos.x, textPos.y, "~~~", NULL);

    textColor = nvgRGB(0xda, 0xe9, 0x29);
    nvgFillColor(vg, nvgTransRGBA(textColor, 16));
    nvgText(vg, textPos.x, textPos.y, "\\\\\\", NULL);

    textColor = nvgRGB(0xf0, 0x00, 0x00);
    nvgFillColor(vg, textColor);
    nvgText(vg, textPos.x, textPos.y, to_display.c_str(), NULL);
  }
};


SEQEuclidWidget::SEQEuclidWidget() {
  SEQEuclid *module = new SEQEuclid();
  setModule(module);
  box.size = Vec(17*22, 380);

  const float bankX[10] = { 8, 94, 134, 220, 258, 296, 324, 351 };
  const float bankY[7] = { 23, 72, 110, 164, 218, 272, 326 };
  
  {
    Panel *panel = new LightPanel();
    panel->backgroundImage = Image::load(BG_IMAGE_FILE);
    panel->box.size = box.size;
    addChild(panel);
  }
  
  // bpm display + control

  {
    SEQEuclidDisplay *display = new SEQEuclidDisplay();
    display->box.pos = Vec(bankX[0], bankY[0]);
    display->box.size = Vec(82, 42);
    display->value = &module->bpm;
    addChild(display);
  }
  addParam(createParam<Davies1900hBlackKnob>(Vec(bankX[1], bankY[0]+3), module, SEQEuclid::BPM_PARAM, 30.0, 256.0, 120.0));

  
  // Next row of stuff

  addInput(createInput<PJ301MPort>(Vec(bankX[0], bankY[1]), module, SEQEuclid::EXT_CLOCK_INPUT));
  addInput(createInput<PJ301MPort>(Vec(bankX[1], bankY[1]), module, SEQEuclid::RESET_INPUT));
  addParam(createParam<TL1105>(Vec(bankX[1]+24+4, bankY[1] + 4), module, SEQEuclid::RESET_BUTTON, 0.0, 1.0, 0.0));
  addParam(createParam<Davies1900hBlackKnob>(Vec(bankX[5]-6, bankY[1] + 3), module, SEQEuclid::GATE_LENGTH_PARAM, 0.0, 1.0, 1.0));

  // Row displays

  {
    SEQEuclidDisplay *display = new SEQEuclidDisplay();
    display->box.pos = Vec(bankX[0], bankY[2]);
    display->box.size = Vec(82, 42);
    display->value = &module->bank1.fill;
    addChild(display);
  }
  {
    SEQEuclidDisplay *display = new SEQEuclidDisplay();
    display->box.pos = Vec(bankX[2], bankY[2]);
    display->box.size = Vec(82, 42);
    display->value = &module->bank1.length;
    addChild(display);
  }

  {
    SEQEuclidDisplay *display = new SEQEuclidDisplay();
    display->box.pos = Vec(bankX[0], bankY[3]);
    display->box.size = Vec(82, 42);
    display->value = &module->bank2.fill;
    addChild(display);
  }
  {
    SEQEuclidDisplay *display = new SEQEuclidDisplay();
    display->box.pos = Vec(bankX[2], bankY[3]);
    display->box.size = Vec(82, 42);
    display->value = &module->bank2.length;
    addChild(display);
  }
  {
    SEQEuclidDisplay *display = new SEQEuclidDisplay();
    display->box.pos = Vec(bankX[0], bankY[4]);
    display->box.size = Vec(82, 42);
    display->value = &module->bank3.fill;
    addChild(display);
  }
  {
    SEQEuclidDisplay *display = new SEQEuclidDisplay();
    display->box.pos = Vec(bankX[2], bankY[4]);
    display->box.size = Vec(82, 42);
    display->value = &module->bank3.length;
    addChild(display);
  }
  {
    SEQEuclidDisplay *display = new SEQEuclidDisplay();
    display->box.pos = Vec(bankX[0], bankY[5]);
    display->box.size = Vec(82, 42);
    display->value = &module->bank4.fill;
    addChild(display);
  }
  {
    SEQEuclidDisplay *display = new SEQEuclidDisplay();
    display->box.pos = Vec(bankX[2], bankY[5]);
    display->box.size = Vec(82, 42);
    display->value = &module->bank4.length;
    addChild(display);
  }


  // Rows of bank controlls

  for (int row = 0; row < 4; row++) {
    addParam(createParam<Davies1900hBlackKnob>(Vec(bankX[1], bankY[row + 2] + 3), module, SEQEuclid::FILL1_PARAM+row, 0.0, 256.0, 0.0));
    addParam(createParam<Davies1900hBlackKnob>(Vec(bankX[3], bankY[row + 2] + 3), module, SEQEuclid::LENGTH1_PARAM+row, 0.0, 256.0, 0.0));
    addParam(createParam<Davies1900hBlackKnob>(Vec(bankX[4], bankY[row + 2] + 3), module, SEQEuclid::PROB1_PARAM + row, 0.0, 1.0, 1.0));
    addOutput(createOutput<PJ301MPort>(Vec(bankX[5], bankY[row + 2] + 9), module, SEQEuclid::GATE1_OUTPUT + row));
    addOutput(createOutput<PJ301MPort>(Vec(bankX[6], bankY[row + 2] + 9), module, SEQEuclid::TRIGGER1_OUTPUT + row));
    addParam(createParam<TL1105>(Vec(bankX[7], bankY[row + 2] + 13), module, SEQEuclid::JOG1_BUTTON + row, 0.0, 1.0, 0.0));
  }
    
  // Final 2 outputs and output light

  addOutput(createOutput<PJ301MPort>(Vec(bankX[5], bankY[6] + 8), module, SEQEuclid::GATE_OR_OUTPUT));
  addOutput(createOutput<PJ301MPort>(Vec(bankX[6], bankY[6] + 8), module, SEQEuclid::TRIGGER_OR_OUTPUT));
  addChild(createValueLight<SmallLight<RedValueLight>>(Vec(bankX[7]+4, bankY[6] + 16), &module->gatesLight));
  
  // Make sure it stays put

  addChild(createScrew<ScrewSilver>(Vec(15, 0)));
  addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 0)));
  addChild(createScrew<ScrewSilver>(Vec(15, 365)));
  addChild(createScrew<ScrewSilver>(Vec(box.size.x-30, 365)));

}
