/*
The filter DSP code has been derived from
Miller Puckette's code hosted at
https://github.com/ddiakopoulos/MoogLadders/blob/master/src/RKSimulationModel.h
which is licensed for use under the following terms (MIT license):


Copyright (c) 2015, Miller Puckette. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#include "Gratrix.hpp"

// The clipping function of a transistor pair is approximately tanh(x)
// TODO: Put this in a lookup table. 5th order approx doesn't seem to cut it
using simd::float_4;

template <typename T>
static T clip(T x) {
	// return std::tanh(x);
	// Pade approximant of tanh
	x = simd::clamp(x, -3.f, 3.f);
	return x * (27 + x * x) / (27 + 9 * x * x);
}

template <typename T>
struct LadderFilter {
	T omega0;
	T resonance = 1;
	T state[4];
	T input;

	LadderFilter() {
		reset();
		setCutoff(0);
	}

	void reset() {
		for (int i = 0; i < 4; i++) {
			state[i] = 0;
		}
	}

	void setCutoff(T cutoff) {
		omega0 = 2 * T(M_PI) * cutoff;
	}

	void process(T input, T dt) {
		dsp::stepRK4(T(0), dt, state, 4, [&](T t, const T x[], T dxdt[]) {
			T inputt = crossfade(this->input, input, t / dt);
			T inputc = clip(inputt - resonance * x[3]);
			T yc0 = clip(x[0]);
			T yc1 = clip(x[1]);
			T yc2 = clip(x[2]);
			T yc3 = clip(x[3]);

			dxdt[0] = omega0 * (inputc - yc0);
			dxdt[1] = omega0 * (yc0 - yc1);
			dxdt[2] = omega0 * (yc1 - yc2);
			dxdt[3] = omega0 * (yc2 - yc3);
		});

		this->input = input;
	}

	T lowpass() {
		return state[3];
	}
	T highpass() {
		// TODO This is incorrect when `resonance > 0`. Is the math wrong?
		return clip((input - resonance * state[3]) - 4 * state[0] + 6 * state[1] - 4 * state[2] + state[3]);
	}
};


//============================================================================================================

struct VCF : GControls::MicroModule {
	enum ParamIds {
		FREQ_PARAM,
		FINE_PARAM,
		RES_PARAM,
		FREQ_CV_PARAM,
		DRIVE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		FREQ_INPUT,
		RES_INPUT,
		DRIVE_INPUT,
		IN_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		LPF_OUTPUT,
		HPF_OUTPUT,
		NUM_OUTPUTS
	};

	LadderFilter<float> filter;

	VCF() : MicroModule(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS) {}
	void step(float s_Rate);
	void onReset() {
		filter.reset();
	}
};


//============================================================================================================

void VCF::step(float s_Rate) {
	float input = inputs[IN_INPUT].getVoltage() / 5.0f;
	float fineParam = params[FINE_PARAM].getValue();
	fineParam = dsp::quadraticBipolar(fineParam * 2.f - 1.f) * 7.f / 12.f;

	float freqCvParam = params[FREQ_CV_PARAM].getValue();
	freqCvParam = dsp::quadraticBipolar(freqCvParam);
	float freqParam = params[FREQ_PARAM].getValue();
	freqParam = freqParam * 10.f - 5.f;

	float driveParam = params[DRIVE_PARAM].getValue();
	float drive = driveParam + inputs[DRIVE_INPUT].getVoltage() / 10.0f;
	float gain = powf(100.0f, drive);
	input *= gain;
	// Add -120dB noise to bootstrap self-oscillation
	input += 1e-6f * (2.0f*random::uniform() - 1.0f);

	// Set resonance
	float res = params[RES_PARAM].getValue() + inputs[RES_INPUT].getVoltage() / 10.0f;
	res = clamp(res, 0.0f, 1.0f);
	filter.resonance = simd::pow(res, 2) * 10.f;

	// Set cutoff frequency
	float cutoffExp = freqParam + fineParam + freqCvParam * inputs[FREQ_INPUT].getVoltage();
	cutoffExp = dsp::FREQ_C4 * simd::pow(2.f, cutoffExp);
	cutoffExp = clamp(cutoffExp, 1.f, 8000.f);
	filter.setCutoff(cutoffExp);


	// Push a sample to the state filter
	filter.process(input, 1.0f/s_Rate);

	// Set outputs
	outputs[LPF_OUTPUT].setVoltage(5.0f * filter.lowpass());
	outputs[HPF_OUTPUT].setVoltage(5.0f * filter.highpass());
}


//============================================================================================================

struct VCFBank : Module
{
	std::array<VCF, GTX__N> inst;

	VCFBank() {
		config(VCF::NUM_PARAMS, (GTX__N+1) * VCF::NUM_INPUTS, GTX__N * VCF::NUM_OUTPUTS);
		configParam(VCF::FREQ_PARAM, 0.0f, 1.0f, 0.5f,  "Frequency", " Hz", std::pow(2, 10.f), dsp::FREQ_C4 / std::pow(2, 5.f));
		configParam(VCF::FINE_PARAM, 0.0f, 1.0f, 0.5f, "Fine frequency");
		configParam(VCF::RES_PARAM, 0.0f, 1.0f, 0.0f, "Resonance", "%", 0.f, 100.f);
		configParam(VCF::FREQ_CV_PARAM, -1.0f, 1.0f, 0.0f,  "Frequency modulation", "%", 0.f, 100.f);
		configParam(VCF::DRIVE_PARAM, 0.0f, 1.0f, 0.0f, "Drive", "", 0, 11);
	}

	static std::size_t imap(std::size_t port, std::size_t bank)
	{
		return port + bank * VCF::NUM_INPUTS;
	}

	static std::size_t omap(std::size_t port, std::size_t bank)
	{
		return port + bank * VCF::NUM_OUTPUTS;
	}

	void process(const ProcessArgs& args) override
	{
		float sample_Rate = args.sampleRate;
		for (std::size_t i=0; i<GTX__N; ++i)
		{
			for (std::size_t p=0; p<VCF::NUM_PARAMS;  ++p) inst[i].params[p]  = params[p];
			for (std::size_t p=0; p<VCF::NUM_INPUTS;  ++p) inst[i].inputs[p]  = inputs[imap(p, i)].isConnected() ? inputs[imap(p, i)] : inputs[imap(p, GTX__N)];
			for (std::size_t p=0; p<VCF::NUM_OUTPUTS; ++p) inst[i].outputs[p] = outputs[omap(p, i)];

			inst[i].step(sample_Rate);

			for (std::size_t p=0; p<VCF::NUM_OUTPUTS; ++p) outputs[omap(p, i)].setVoltage(inst[i].outputs[p].value);
		}
	}

	void onReset() override
	{
		for (std::size_t i=0; i<GTX__N; ++i)
		{
			inst[i].onReset();
		}
	}
};


//============================================================================================================
//! \brief The widget.

struct GtxWidget : ModuleWidget
{
	GtxWidget(VCFBank *module)
	{
		setModule(module);
		box.size = Vec(18*15, 380);

		#if GTX__SAVE_SVG
		{
			PanelGen pg(asset::plugin(pluginInstance, "build/res/VCF-F1.svg"), box.size, "VCF-F1");

			pg.nob_big(0, 0, "FREQ");

			pg.nob_med(1.1, -0.28, "FINE");     pg.nob_med(1.9, -0.28, "RES");
			pg.nob_med(1.1, +0.28, "FREQ  CV"); pg.nob_med(1.9, +0.28, "DRIVE");

			pg.bus_in(0, 1, "FREQ"); pg.bus_in(1, 1, "RES");   pg.bus_out(2, 1, "HPF");
			pg.bus_in(0, 2, "IN");   pg.bus_in(1, 2, "DRIVE"); pg.bus_out(2, 2, "LPF");
		}
		#endif

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/VCF-F1.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

		addParam(createParamCentered<GControls::KnobFreeHug>(Vec(GControls::fx(0.0), GControls::fy(+0.00)), module, VCF::FREQ_PARAM));
		addParam(createParamCentered<GControls::KnobFreeMed>(Vec(GControls::fx(1.1), GControls::fy(-0.28)), module, VCF::FINE_PARAM));
		addParam(createParamCentered<GControls::KnobFreeMed>(Vec(GControls::fx(1.9), GControls::fy(-0.28)), module, VCF::RES_PARAM));
		addParam(createParamCentered<GControls::KnobFreeMed>(Vec(GControls::fx(1.1), GControls::fy(+0.28)), module, VCF::FREQ_CV_PARAM));
		addParam(createParamCentered<GControls::KnobFreeMed>(Vec(GControls::fx(1.9), GControls::fy(+0.28)), module, VCF::DRIVE_PARAM));

		for (std::size_t i=0; i<GTX__N; ++i)
		{
			addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::px(0, i), GControls::py(1, i)), module, VCFBank::imap(VCF::FREQ_INPUT,  i)));
			addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::px(1, i), GControls::py(1, i)), module, VCFBank::imap(VCF::RES_INPUT,   i)));
			addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::px(1, i), GControls::py(2, i)), module, VCFBank::imap(VCF::DRIVE_INPUT, i)));
			addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::px(0, i), GControls::py(2, i)), module, VCFBank::imap(VCF::IN_INPUT,    i)));

			addOutput(createOutputCentered<GControls::PortOutMed>(Vec(GControls::px(2, i), GControls::py(2, i)), module, VCFBank::omap(VCF::LPF_OUTPUT,  i)));
			addOutput(createOutputCentered<GControls::PortOutMed>(Vec(GControls::px(2, i), GControls::py(1, i)), module, VCFBank::omap(VCF::HPF_OUTPUT,  i)));
		}

		addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::gx(0), GControls::gy(1)), module, VCFBank::imap(VCF::FREQ_INPUT,  GTX__N)));
		addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::gx(1), GControls::gy(1)), module, VCFBank::imap(VCF::RES_INPUT,   GTX__N)));
		addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::gx(1), GControls::gy(2)), module, VCFBank::imap(VCF::DRIVE_INPUT, GTX__N)));
		addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::gx(0), GControls::gy(2)), module, VCFBank::imap(VCF::IN_INPUT,    GTX__N)));
	}
};


Model *modelVCF_F1 = createModel<VCFBank, GtxWidget>("VCF-F1");
