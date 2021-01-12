//============================================================================================================
//!
//! \file Fade-G1.cpp
//!
//! \brief Fade-G1 is a two input six voice one-dimensional fader.
//!
//============================================================================================================

#include "Gratrix.hpp"
//============================================================================================================
//! \brief The module.

struct Fade_G1 : Module
{
	enum ParamIds {
		BLEND12_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		BLEND12_INPUT,
		IN1_INPUT,
		IN2_INPUT,
		NUM_INPUTS,
		OFF_INPUTS = IN1_INPUT
	};
	enum OutputIds {
		OUT1_OUTPUT,
		OUT2_OUTPUT,
		NUM_OUTPUTS,
		OFF_OUTPUTS = OUT1_OUTPUT
	};
	enum LightIds {
		IN_1_GREEN,  IN_1_RED,
		IN_2_GREEN,  IN_2_RED,
		OUT_1_GREEN, OUT_1_RED,
		OUT_2_GREEN, OUT_2_RED,
		NUM_LIGHTS
	};

	Fade_G1() {
		config(NUM_PARAMS, (GTX__N+1) * (NUM_INPUTS  - OFF_INPUTS ) + OFF_INPUTS,(GTX__N  ) * (NUM_OUTPUTS - OFF_OUTPUTS) + OFF_OUTPUTS, NUM_LIGHTS);
		configParam(BLEND12_PARAM, 0.f, 1.f, 0.5f, "Fade");
		lights[IN_1_GREEN].value = 0.0f;  lights[IN_1_RED].value = 1.0f;
		lights[IN_2_GREEN].value = 1.0f;  lights[IN_2_RED].value = 0.0f;
	}

	static constexpr std::size_t imap(std::size_t port, std::size_t bank)
	{
		return (port < OFF_INPUTS) ? port : port + bank * (NUM_INPUTS - OFF_INPUTS);
	}

	static constexpr std::size_t omap(std::size_t port, std::size_t bank)
	{
		return port + bank * NUM_OUTPUTS;
	}

	void process(const ProcessArgs& args) override
	{
		float blend12 = params[BLEND12_PARAM].getValue();

		if (inputs[BLEND12_INPUT].isConnected()) blend12 *= clamp(inputs[BLEND12_INPUT].getNormalVoltage(10.0f) / 10.0f, 0.0f, 1.0f);

		for (std::size_t i=0; i<GTX__N; ++i)
		{
			float input1 = inputs[imap(IN1_INPUT, i)].isConnected() ? inputs[imap(IN1_INPUT, i)].getVoltage() : inputs[imap(IN1_INPUT, GTX__N)].getVoltage();
			float input2 = inputs[imap(IN2_INPUT, i)].isConnected() ? inputs[imap(IN2_INPUT, i)].getVoltage() : inputs[imap(IN2_INPUT, GTX__N)].getVoltage();

			float delta12 = blend12 * (input2 - input1);

			outputs[omap(OUT1_OUTPUT, i)].setVoltage(input1 + delta12);
			outputs[omap(OUT2_OUTPUT, i)].setVoltage(input2 - delta12);
		}

		lights[OUT_1_GREEN].value = lights[OUT_2_RED].value =        blend12;
		lights[OUT_2_GREEN].value = lights[OUT_1_RED].value = 1.0f - blend12;
	}
};


//============================================================================================================
//! \brief The widget.

struct GtxWidget : ModuleWidget
{
	GtxWidget(Fade_G1 *module)
	{
		setModule(module);
		box.size = Vec(12*15, 380);

		#if GTX__SAVE_SVG
		{
			PanelGen pg(asset::plugin(pluginInstance, "build/res/Fade-G1.svg"), box.size, "FADE-G1");

			// ― is a horizontal bar see https://en.wikipedia.org/wiki/Dash#Horizontal_bar
			pg.prt_in2(0, 0, "CV 1―2");   pg.nob_big(1, 0, "1―2");

			pg.bus_in(0, 1, "IN 1"); pg.bus_out(1, 1, "OUT 1");
			pg.bus_in(0, 2, "IN 2"); pg.bus_out(1, 2, "OUT 2");
		}
		#endif

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Fade-G1.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

		if(module)
		{
			addParam(createParamCentered<GControls::KnobFreeHug>(Vec(GControls::fx(1), GControls::fy(0)), module, Fade_G1::BLEND12_PARAM));
		}

		addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::fx(0), GControls::fy(0)), module, Fade_G1::BLEND12_INPUT));

		for (std::size_t i=0; i<GTX__N; ++i)
		{
			addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::px(0, i), GControls::py(1, i)), module, Fade_G1::imap(Fade_G1::IN1_INPUT, i)));
			addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::px(0, i), GControls::py(2, i)), module, Fade_G1::imap(Fade_G1::IN2_INPUT, i)));

			addOutput(createOutputCentered<GControls::PortOutMed>(Vec(GControls::px(1, i), GControls::py(1, i)), module, Fade_G1::omap(Fade_G1::OUT1_OUTPUT, i)));
			addOutput(createOutputCentered<GControls::PortOutMed>(Vec(GControls::px(1, i), GControls::py(2, i)), module, Fade_G1::omap(Fade_G1::OUT2_OUTPUT, i)));
		}

		addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::gx(0), GControls::gy(1)), module, Fade_G1::imap(Fade_G1::IN1_INPUT, GTX__N)));
		addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::gx(0), GControls::gy(2)), module, Fade_G1::imap(Fade_G1::IN2_INPUT, GTX__N)));

		for (std::size_t i=0, x=0; x<2; ++x)
		{
			for (std::size_t y=0; y<2; ++y)
			{
				addChild(createLight<SmallLight<GreenRedLight>>(GControls::l_s(GControls::gx(x)+28, GControls::gy(y+1)-47.5), module, i)); i+=2;
			}
		}
	}
};


Model *modelFade_G1 = createModel<Fade_G1, GtxWidget>("Fade-G1");