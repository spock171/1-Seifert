#include "Gratrix.hpp"

//============================================================================================================

struct VCA : GControls::MicroModule {
	enum ParamIds {
		LEVEL_PARAM,
		MIX_1_PARAM,
		MIX_2_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		EXP_INPUT,     // N+1
		LIN_INPUT,     // N+1
		IN_INPUT,      // N+1
		NUM_INPUTS,
		OFF_INPUTS = EXP_INPUT
	};
	enum OutputIds {
		MIX_1_OUTPUT,  // 1
		MIX_2_OUTPUT,  // 1
		OUT_OUTPUT,    // N
		NUM_OUTPUTS,
		OFF_OUTPUTS = OUT_OUTPUT
	};

	VCA() : MicroModule(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {}
	void step();
};


//============================================================================================================

void VCA::step() {
	float v = inputs[IN_INPUT].getVoltage() * params[LEVEL_PARAM].getValue();
	if (inputs[LIN_INPUT].isConnected())
		v *= clamp(inputs[LIN_INPUT].getVoltage() / 10.0f, 0.0f, 1.0f);
	const float expBase = 50.0f;
	if (inputs[EXP_INPUT].isConnected())
		v *= rescale(powf(expBase, clamp(inputs[EXP_INPUT].getVoltage() / 10.0f, 0.0f, 1.0f)), 1.0f, expBase, 0.0f, 1.0f);
	outputs[OUT_OUTPUT].setVoltage(v);
}


//============================================================================================================

struct VCABank : Module
{
	std::array<VCA, GTX__N> inst;

	VCABank() {config(VCA::NUM_PARAMS,
			(GTX__N+1) * (VCA::NUM_INPUTS  - VCA::OFF_INPUTS ) + VCA::OFF_INPUTS,
			(GTX__N  ) * (VCA::NUM_OUTPUTS - VCA::OFF_OUTPUTS) + VCA::OFF_OUTPUTS);
	}

	static constexpr std::size_t imap(std::size_t port, std::size_t bank)
	{
		return port + bank * VCA::NUM_INPUTS;
	}

	static constexpr std::size_t omap(std::size_t port, std::size_t bank)
	{
		return (port < VCA::OFF_OUTPUTS) ? port : port + bank * (VCA::NUM_OUTPUTS - VCA::OFF_OUTPUTS);
	}

	void process(const ProcessArgs& args) override
	{
		for (std::size_t i=0; i<GTX__N; ++i)
		{
			for (std::size_t p=0; p<VCA::NUM_PARAMS;  ++p) inst[i].params[p]  = params[p];
			for (std::size_t p=0; p<VCA::NUM_INPUTS;  ++p) inst[i].inputs[p]  = inputs[imap(p, i)].isConnected() ? inputs[imap(p, i)] : inputs[imap(p, GTX__N)];
			for (std::size_t p=0; p<VCA::NUM_OUTPUTS; ++p) inst[i].outputs[p] = outputs[omap(p, i)];

			inst[i].step();

			for (std::size_t p=0; p<VCA::NUM_OUTPUTS; ++p) outputs[omap(p, i)].setVoltage(inst[i].outputs[p].value);
		}

		float mix = 0.0f;

		for (std::size_t i=0; i<GTX__N; ++i)
		{
			mix += inst[i].outputs[VCA::OUT_OUTPUT].value;
		}

		outputs[VCA::MIX_1_OUTPUT].setVoltage(mix * params[VCA::MIX_1_PARAM].getValue());
		outputs[VCA::MIX_2_OUTPUT].setVoltage(mix * params[VCA::MIX_2_PARAM].getValue());
	}
};


//============================================================================================================
//! \brief The widget.

struct GtxWidget : ModuleWidget
{
	GtxWidget(VCABank *module)
	{
		setModule(module);
		box.size = Vec(12*15, 380);

		#if GTX__SAVE_SVG
		{
			PanelGen pg(asset::plugin(pluginInstance, "build/res/VCA-F1.svg"), box.size, "VCA-F1");

			pg.nob_big(0, 0, "LEVEL");

			pg.nob_med(1, -0.28, "MIX OUT 1");
			pg.nob_med(1, +0.28, "MIX OUT 2");

			pg.bus_in (0, 1, "EXP"); pg.bus_in (1, 1, "LIN");
			pg.bus_in (0, 2, "IN");  pg.bus_out(1, 2, "OUT");
		}
		#endif

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/VCA-F1.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

		addParam(createParamCentered<GControls::KnobFreeHug>(Vec(GControls::fx(0),      GControls::fy(0)),     module, VCA::LEVEL_PARAM));
		addParam(createParamCentered<GControls::KnobFreeMed>(Vec(GControls::fx(1-0.18), GControls::fy(-0.28)), module, VCA::MIX_1_PARAM));
		addParam(createParamCentered<GControls::KnobFreeMed>(Vec(GControls::fx(1-0.18), GControls::fy(+0.28)), module, VCA::MIX_2_PARAM));

		addOutput(createOutputCentered<GControls::PortOutMed>(Vec(GControls::fx(1+0.28), GControls::fy(-0.28)), module, VCA::MIX_1_OUTPUT));
		addOutput(createOutputCentered<GControls::PortOutMed>(Vec(GControls::fx(1+0.28), GControls::fy(+0.28)), module, VCA::MIX_2_OUTPUT));

		for (std::size_t i=0; i<GTX__N; ++i)
		{
			addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::px(1, i), GControls::py(1, i)), module, VCABank::imap(VCA::LIN_INPUT, i)));
			addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::px(0, i), GControls::py(1, i)), module, VCABank::imap(VCA::EXP_INPUT, i)));
			addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::px(0, i), GControls::py(2, i)), module, VCABank::imap(VCA::IN_INPUT,  i)));

			addOutput(createOutputCentered<GControls::PortOutMed>(Vec(GControls::px(1, i), GControls::py(2, i)), module, VCABank::omap(VCA::OUT_OUTPUT, i)));
		}

		addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::gx(1), GControls::gy(1)), module, VCABank::imap(VCA::LIN_INPUT, GTX__N)));
		addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::gx(0), GControls::gy(1)), module, VCABank::imap(VCA::EXP_INPUT, GTX__N)));
		addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::gx(0), GControls::gy(2)), module, VCABank::imap(VCA::IN_INPUT,  GTX__N)));
	}
};


Model *modelVCA_F1 = createModel<VCABank, GtxWidget>("VCA-F1");
