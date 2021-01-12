//============================================================================================================
//!
//! \file Blank06.cpp
//!
//! \brief Blank 6 is a simple do nothing 6-hole high quality blank.
//!
//============================================================================================================


#include "Gratrix.hpp"

struct Blank_06 : Module
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

	Blank_06() {config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);}
};


//============================================================================================================
//! \brief The widget.

struct GtxWidget : ModuleWidget
{
	GtxWidget(Blank_06 *module)
	{
		setModule(module);
		box.size = Vec(6*15, 380);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Blank06.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));
	}
};


Model *modelBlank_06 = createModel<Blank_06, GtxWidget>("Blank-06");
