#include "Gratrix.hpp"

//============================================================================================================

struct ADSR : GControls::MicroModule {
	enum ParamIds {
		ATTACK_PARAM,
		DECAY_PARAM,
		SUSTAIN_PARAM,
		RELEASE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		ATTACK_INPUT,     // 1
		DECAY_INPUT,      // 1
		SUSTAIN_INPUT,    // 1
		RELEASE_INPUT,    // 1
		GATE_INPUT,       // N+1
		TRIG_INPUT,       // N+1
		NUM_INPUTS,
		OFF_INPUTS = GATE_INPUT
	};
	enum OutputIds {
		ENVELOPE_OUTPUT,  // N
		INVERTED_OUTPUT,  // N
		NUM_OUTPUTS,
		OFF_OUTPUTS = ENVELOPE_OUTPUT
	};
	enum LightIds {
		NUM_LIGHTS
	};

	bool decaying = false;
	float env = 0.0f;
	dsp::SchmittTrigger trigger;

	ADSR() : MicroModule(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS) {}
	void step();
};


//============================================================================================================

void ADSR::step() {
	float attack = clamp(params[ATTACK_INPUT].getValue() + inputs[ATTACK_INPUT].getVoltage() / 10.0f, 0.0f, 1.0f);
	float decay = clamp(params[DECAY_PARAM].getValue() + inputs[DECAY_INPUT].getVoltage() / 10.0f, 0.0f, 1.0f);
	float sustain = clamp(params[SUSTAIN_PARAM].getValue() + inputs[SUSTAIN_INPUT].getVoltage() / 10.0f, 0.0f, 1.0f);
	float release = clamp(params[RELEASE_PARAM].getValue() + inputs[RELEASE_PARAM].getVoltage() / 10.0f, 0.0f, 1.0f);

	// Gate and trigger
	bool gated = inputs[GATE_INPUT].getVoltage() >= 1.0f;
	if (trigger.process(inputs[TRIG_INPUT].getVoltage()))
		decaying = false;

	const float base = 20000.0f;
	const float maxTime = 10.0f;
	if (gated) {
		if (decaying) {
			// Decay
			if (decay < 1e-4) {
				env = sustain;
			}
			else {
				env += powf(base, 1 - decay) / maxTime * (sustain - env) * APP->engine->getSampleTime();
			}
		}
		else {
			// Attack
			// Skip ahead if attack is all the way down (infinitely fast)
			if (attack < 1e-4) {
				env = 1.0f;
			}
			else {
				env += powf(base, 1 - attack) / maxTime * (1.01f - env) * APP->engine->getSampleTime();
			}
			if (env >= 1.0f) {
				env = 1.0f;
				decaying = true;
			}
		}
	}
	else {
		// Release
		if (release < 1e-4) {
			env = 0.0f;
		}
		else {
			env += powf(base, 1 - release) / maxTime * (0.0f - env) * APP->engine->getSampleTime();
		}
		decaying = false;
	}

	outputs[ENVELOPE_OUTPUT].setVoltage(10.0 * env);
	outputs[INVERTED_OUTPUT].setVoltage(10.0 * (1.0 - env));
}


//============================================================================================================

struct ADSR_F1 : Module
{
	std::array<ADSR, GTX__N> inst;

	ADSR_F1() {
		config(ADSR::NUM_PARAMS,
			(GTX__N+1) * (ADSR::NUM_INPUTS  - ADSR::OFF_INPUTS ) + ADSR::OFF_INPUTS,
			(GTX__N  ) * (ADSR::NUM_OUTPUTS - ADSR::OFF_OUTPUTS) + ADSR::OFF_OUTPUTS);
		configParam(ADSR::ATTACK_PARAM,  0.0, 1.0, 0.5, "Attack");
		configParam(ADSR::DECAY_PARAM,   0.0, 1.0, 0.5, "Decay");
		configParam(ADSR::SUSTAIN_PARAM, 0.0, 1.0, 0.5, "Sustain");
		configParam(ADSR::RELEASE_PARAM, 0.0, 1.0, 0.5, "Release");
	}

	static constexpr std::size_t imap(std::size_t port, std::size_t bank)
	{
		return (port < ADSR::OFF_INPUTS)  ? port : port + bank * (ADSR::NUM_INPUTS  - ADSR::OFF_INPUTS);
	}

	static constexpr std::size_t omap(std::size_t port, std::size_t bank)
	{
	//	return (port < ADSR::OFF_OUTPUTS) ? port : port + bank * (ADSR::NUM_OUTPUTS - ADSR::OFF_OUTPUTS);
		return                                     port + bank *  ADSR::NUM_OUTPUTS;
	}

	void process(const ProcessArgs& args) override
	{
		for (std::size_t i=0; i<GTX__N; ++i)
		{
			for (std::size_t p=0; p<ADSR::NUM_PARAMS;  ++p) inst[i].params[p]  = params[p];
			for (std::size_t p=0; p<ADSR::NUM_INPUTS;  ++p) inst[i].inputs[p]  = inputs[imap(p, i)].isConnected() ? inputs[imap(p, i)] : inputs[imap(p, GTX__N)];
			for (std::size_t p=0; p<ADSR::NUM_OUTPUTS; ++p) inst[i].outputs[p] = outputs[omap(p, i)];

			inst[i].step();

			for (std::size_t p=0; p<ADSR::NUM_OUTPUTS; ++p) outputs[omap(p, i)].setVoltage(inst[i].outputs[p].value);
		}
	}
};


//============================================================================================================

struct GtxWidget : ModuleWidget
{
	GtxWidget(ADSR_F1 *module)
	{
		setModule(module);
		box.size = Vec(12*15, 380);

		#if GTX__SAVE_SVG
		{
			PanelGen pg(asset::plugin(pluginInstance, "build/res/ADSR-F1.svg"), box.size, "ADSR-F1");

			pg.nob_med(0, -0.28, "ATTACK");  pg.nob_med(1, -0.28, "DECAY");
			pg.nob_med(0, +0.28, "SUSTAIN"); pg.nob_med(1, +0.28, "RELEASE");

			pg.bus_in(0, 1, "GATE");  pg.bus_out(1, 1, "OUT");
			pg.bus_in(0, 2, "TRIG");  pg.bus_out(1, 2, "INV OUT");
		}
		#endif

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/ADSR-F1.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

		addParam(createParamCentered<GControls::KnobFreeMed>(Vec(GControls::fx(0+0.18), GControls::fy(-0.28)), module, ADSR::ATTACK_PARAM));
		addParam(createParamCentered<GControls::KnobFreeMed>(Vec(GControls::fx(1+0.18), GControls::fy(-0.28)), module, ADSR::DECAY_PARAM));
		addParam(createParamCentered<GControls::KnobFreeMed>(Vec(GControls::fx(0+0.18), GControls::fy(+0.28)), module, ADSR::SUSTAIN_PARAM));
		addParam(createParamCentered<GControls::KnobFreeMed>(Vec(GControls::fx(1+0.18), GControls::fy(+0.28)), module, ADSR::RELEASE_PARAM));

		addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::fx(0-0.28), GControls::fy(-0.28)), module, ADSR::ATTACK_INPUT));
		addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::fx(1-0.28), GControls::fy(-0.28)), module, ADSR::DECAY_INPUT));
		addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::fx(0-0.28), GControls::fy(+0.28)), module, ADSR::SUSTAIN_INPUT));
		addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::fx(1-0.28), GControls::fy(+0.28)), module, ADSR::RELEASE_INPUT));

		for (std::size_t i=0; i<GTX__N; ++i)
		{
			addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::px(0, i), GControls::py(1, i)), module, ADSR_F1::imap(ADSR::GATE_INPUT, i)));
			addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::px(0, i), GControls::py(2, i)), module, ADSR_F1::imap(ADSR::TRIG_INPUT, i)));

			addOutput(createOutputCentered<GControls::PortOutMed>(Vec(GControls::px(1, i), GControls::py(1, i)), module, ADSR_F1::omap(ADSR::ENVELOPE_OUTPUT, i)));
			addOutput(createOutputCentered<GControls::PortOutMed>(Vec(GControls::px(1, i), GControls::py(2, i)), module, ADSR_F1::omap(ADSR::INVERTED_OUTPUT, i)));
		}

		addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::gx(0), GControls::gy(1)), module, ADSR_F1::imap(ADSR::GATE_INPUT, GTX__N)));
		addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::gx(0), GControls::gy(2)), module, ADSR_F1::imap(ADSR::TRIG_INPUT, GTX__N)));
	}
};


Model *modelADSR_F1 = createModel<ADSR_F1, GtxWidget>("ADSR-F1");
