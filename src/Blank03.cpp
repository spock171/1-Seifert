//============================================================================================================
//!
//! \file Blank03.cpp
//!
//! \brief Blank 3 is a simple do nothing 3-hole high quality blank.
//!
//============================================================================================================


#include "Gratrix.hpp"

struct Blank_03 : Module
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

	Blank_03() {config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);}
};


//============================================================================================================
//! \brief The widget.

struct GtxWidget : ModuleWidget
{
	GtxWidget(Blank_03 *module)
	{
		setModule(module);
		box.size = Vec(3*15, 380);

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Blank03.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));
	}
};


Model *modelBlank_03 = createModel<Blank_03, GtxWidget>("Blank-03");
