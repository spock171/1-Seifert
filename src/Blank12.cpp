//============================================================================================================
//!
//! \file Blank12.cpp
//!
//! \brief Blank 12 is a simple do nothing 12-hole high quality blank.
//!
//============================================================================================================


#include "Gratrix.hpp"

struct Blank_12 : Module
{
    enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		BLINK_LIGHT,
		OUTPUT_LIGHT,
		NUM_LIGHTS
	};

	Blank_12() {config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);}
};


//============================================================================================================
//! \brief The widget.

struct GtxWidget : ModuleWidget
{
	GtxWidget(Blank_12 *module)
	{
		setModule(module);
		box.size = Vec(12*15, 380);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Blank12.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));
	}
};


Model *modelBlank_12 = createModel<Blank_12, GtxWidget>("Blank-12");
