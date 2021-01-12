//============================================================================================================
//!
//! \file Fade-G2.cpp
//!
//! \brief Fade-G2 is a four input six voice two-dimensional fader.
//!
//============================================================================================================


#include "Gratrix.hpp"

//============================================================================================================
//! \brief The module.

struct Fade_G2 : Module
{
	enum ParamIds {
		BLEND12_PARAM,
		BLENDAB_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		BLEND12_INPUT,
		BLENDAB_INPUT,
		IN1A_INPUT,
		IN1B_INPUT,
		IN2A_INPUT,
		IN2B_INPUT,
		NUM_INPUTS,
		OFF_INPUTS = IN1A_INPUT
	};
	enum OutputIds {
		OUT1A_OUTPUT,
		OUT2B_OUTPUT,
		NUM_OUTPUTS,
		OFF_OUTPUTS = OUT1A_OUTPUT
	};
	enum LightIds {
		IN_1AP_GREEN,  IN_1AP_RED,
		IN_1AQ_GREEN,  IN_1AQ_RED,
		IN_1BP_GREEN,  IN_1BP_RED,
		IN_1BQ_GREEN,  IN_1BQ_RED,
		IN_2AP_GREEN,  IN_2AP_RED,
		IN_2AQ_GREEN,  IN_2AQ_RED,
		IN_2BP_GREEN,  IN_2BP_RED,
		IN_2BQ_GREEN,  IN_2BQ_RED,
		OUT_1AP_GREEN, OUT_1AP_RED,
		OUT_1AQ_GREEN, OUT_1AQ_RED,
		OUT_2BP_GREEN, OUT_2BP_RED,
		OUT_2BQ_GREEN, OUT_2BQ_RED,
		NUM_LIGHTS
	};

	Fade_G2() {
		config(NUM_PARAMS,
		(GTX__N+1) * (NUM_INPUTS  - OFF_INPUTS ) + OFF_INPUTS,
		(GTX__N  ) * (NUM_OUTPUTS - OFF_OUTPUTS) + OFF_OUTPUTS,
		NUM_LIGHTS);
		lights[IN_1AP_GREEN].value = 0.0f;  lights[IN_1AP_RED].value = 1.0f;
		lights[IN_1AQ_GREEN].value = 0.0f;  lights[IN_1AQ_RED].value = 1.0f;
		lights[IN_1BP_GREEN].value = 1.0f;  lights[IN_1BP_RED].value = 0.0f;
		lights[IN_1BQ_GREEN].value = 0.0f;  lights[IN_1BQ_RED].value = 1.0f;
		lights[IN_2AP_GREEN].value = 0.0f;  lights[IN_2AP_RED].value = 1.0f;
		lights[IN_2AQ_GREEN].value = 1.0f;  lights[IN_2AQ_RED].value = 0.0f;
		lights[IN_2BP_GREEN].value = 1.0f;  lights[IN_2BP_RED].value = 0.0f;
		lights[IN_2BQ_GREEN].value = 1.0f;  lights[IN_2BQ_RED].value = 0.0f;
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
		float blendAB = params[BLENDAB_PARAM].getValue();

		if (inputs[BLEND12_INPUT].isConnected()) blend12 *= clamp(inputs[BLEND12_INPUT].getNormalVoltage(10.0f) / 10.0f, 0.0f, 1.0f);
		if (inputs[BLENDAB_INPUT].isConnected()) blendAB *= clamp(inputs[BLENDAB_INPUT].getNormalVoltage(10.0f) / 10.0f, 0.0f, 1.0f);

		for (std::size_t i=0; i<GTX__N; ++i)
		{
			float input1A  = inputs[imap(IN1A_INPUT, i)].isConnected() ? inputs[imap(IN1A_INPUT, i)].getVoltage() : inputs[imap(IN1A_INPUT, GTX__N)].getVoltage();
			float input1B  = inputs[imap(IN1B_INPUT, i)].isConnected() ? inputs[imap(IN1B_INPUT, i)].getVoltage() : inputs[imap(IN1B_INPUT, GTX__N)].getVoltage();
			float input2A  = inputs[imap(IN2A_INPUT, i)].isConnected() ? inputs[imap(IN2A_INPUT, i)].getVoltage() : inputs[imap(IN2A_INPUT, GTX__N)].getVoltage();
			float input2B  = inputs[imap(IN2B_INPUT, i)].isConnected() ? inputs[imap(IN2B_INPUT, i)].getVoltage() : inputs[imap(IN2B_INPUT, GTX__N)].getVoltage();

			float delta1AB = blendAB * (input1B - input1A);
			float delta2AB = blendAB * (input2B - input2A);

			float temp_1A  = input1A + delta1AB;
			float temp_1B  = input1B - delta1AB;
			float temp_2A  = input2A + delta2AB;
			float temp_2B  = input2B - delta2AB;

			float delta12A = blend12 * (temp_2A - temp_1A);
			float delta12B = blend12 * (temp_2B - temp_1B);

			outputs[omap(OUT1A_OUTPUT, i)].setVoltage(temp_1A + delta12A);
			outputs[omap(OUT2B_OUTPUT, i)].setVoltage(temp_2B - delta12B);
		}

		lights[OUT_1AP_GREEN].value = lights[OUT_2BP_RED].value =        blendAB;
		lights[OUT_1AQ_GREEN].value = lights[OUT_2BQ_RED].value =        blend12;
		lights[OUT_2BP_GREEN].value = lights[OUT_1AP_RED].value = 1.0f - blendAB;
		lights[OUT_2BQ_GREEN].value = lights[OUT_1AQ_RED].value = 1.0f - blend12;
	}
};


//============================================================================================================
//! \brief The widget.

struct GtxWidget : ModuleWidget
{
	GtxWidget(Fade_G2 *module)
	{
		setModule(module);
		box.size = Vec(18*15, 380);

		#if GTX__SAVE_SVG
		{
			PanelGen pg(asset::plugin(pluginInstance, "build/res/Fade-G2.svg"), box.size, "FADE-G2");

			// ― is a horizontal bar see https://en.wikipedia.org/wiki/Dash#Horizontal_bar
			pg.prt_in2(0, -0.28, "CV 1―2");   pg.nob_big(1, 0, "1―2");
			pg.prt_in2(0, +0.28, "CV A―B");   pg.nob_big(2, 0, "A―B");

			pg.bus_in(0, 1, "IN 1A"); pg.bus_out(2, 1, "OUT");
			pg.bus_in(1, 1, "IN 1B");
			pg.bus_in(0, 2, "IN 2A");
			pg.bus_in(1, 2, "IN 2B"); pg.bus_out(2, 2, "INV OUT");
		}
		#endif

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Fade-G2.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

		addParam(createParamCentered<GControls::KnobFreeHug>(Vec(GControls::fx(1), GControls::fy(0)), module, Fade_G2::BLENDAB_PARAM));
		addParam(createParamCentered<GControls::KnobFreeHug>(Vec(GControls::fx(2), GControls::fy(0)), module, Fade_G2::BLEND12_PARAM));

		addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::fx(0), GControls::fy(-0.28)), module, Fade_G2::BLENDAB_INPUT));
		addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::fx(0), GControls::fy(+0.28)), module, Fade_G2::BLEND12_INPUT));

		for (std::size_t i=0; i<GTX__N; ++i)
		{
			addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::px(0, i), GControls::py(1, i)), module, Fade_G2::imap(Fade_G2::IN1A_INPUT, i)));
			addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::px(0, i), GControls::py(2, i)), module, Fade_G2::imap(Fade_G2::IN1B_INPUT, i)));
			addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::px(1, i), GControls::py(1, i)), module, Fade_G2::imap(Fade_G2::IN2A_INPUT, i)));
			addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::px(1, i), GControls::py(2, i)), module, Fade_G2::imap(Fade_G2::IN2B_INPUT, i)));

			addOutput(createOutputCentered<GControls::PortOutMed>(Vec(GControls::px(2, i), GControls::py(1, i)), module, Fade_G2::omap(Fade_G2::OUT1A_OUTPUT, i)));
			addOutput(createOutputCentered<GControls::PortOutMed>(Vec(GControls::px(2, i), GControls::py(2, i)), module, Fade_G2::omap(Fade_G2::OUT2B_OUTPUT, i)));
		}

		addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::gx(0), GControls::gy(1)), module, Fade_G2::imap(Fade_G2::IN1A_INPUT, GTX__N)));
		addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::gx(0), GControls::gy(2)), module, Fade_G2::imap(Fade_G2::IN1B_INPUT, GTX__N)));
		addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::gx(1), GControls::gy(1)), module, Fade_G2::imap(Fade_G2::IN2A_INPUT, GTX__N)));
		addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::gx(1), GControls::gy(2)), module, Fade_G2::imap(Fade_G2::IN2B_INPUT, GTX__N)));

		for (std::size_t i=0, x=0; x<3; ++x)
		{
			for (std::size_t y=0; y<2; ++y)
			{
				addChild(createLight<SmallLight<GreenRedLight>>(GControls::l_s(GControls::gx(x)+GControls::rad_l_s()/2+28, GControls::gy(y+1)-47.5-(1+GControls::rad_l_s())), module, i));
				i+=2;
				addChild(createLight<SmallLight<GreenRedLight>>(GControls::l_s(GControls::gx(x)+GControls::rad_l_s()/2+28, GControls::gy(y+1)-47.5+(1+GControls::rad_l_s())), module, i));
				i+=2;
			}
		}
	}
};


Model *modelFade_G2 = createModel<Fade_G2, GtxWidget>("Fade-G2");