//============================================================================================================
//!
//! \file Chord-G1.cpp
//!
//! \brief Chord-G1 genearates chords via a CV program selection and a fundamental bass note V/octave input.
//!
//============================================================================================================

#include "Gratrix.hpp"

//============================================================================================================
//! \brief Some settings.

enum Spec
{
	E = 12,    // Number of Patterns
	T = 25,    // Number of Notes to choose
	N = GTX__N
};


//============================================================================================================
//! \brief The module.

struct Chord_G1 : Module
{
	enum ParamIds {
		PROG_PARAM = 0,
		NOTE_PARAM  = PROG_PARAM + 1,
		NUM_PARAMS  = NOTE_PARAM + T
	};
	enum InputIds {
		PROG_INPUT,    // 1
		GATE_INPUT,    // 1
		VOCT_INPUT,    // 1
		NUM_INPUTS,
		OFF_INPUTS = VOCT_INPUT
	};
	enum OutputIds {
		GATE_OUTPUT,   // N
		VOCT_OUTPUT,   // N
		NUM_OUTPUTS,
		OFF_OUTPUTS = GATE_OUTPUT
	};
	enum LightIds {
		PROG_LIGHT = 0,
		NOTE_LIGHT = PROG_LIGHT + 2*E,
		FUND_LIGHT = NOTE_LIGHT + 2*T,
		NUM_LIGHTS = FUND_LIGHT + E
	};

	struct Decode
	{
		/*static constexpr*/ float e = static_cast<float>(E);  // Static constexpr gives
		/*static constexpr*/ float s = 1.0f / e;               // link error on Mac build.

		float in    = 0;  //!< Raw input.
		float out   = 0;  //!< Input quantized.
		int   note  = 0;  //!< Integer note (offset midi note).
		int   key   = 0;  //!< C, C#, D, D#, etc.
		int   oct   = 0;  //!< Octave (C4 = 0).

		void step(float input)
		{
			int safe, fnote;

			in    = input;
			fnote = std::floor(in * e + 0.5f);
			out   = fnote * s;
			note  = static_cast<int>(fnote);
			safe  = note + (E * 1000);  // push away from negative numbers
			key   = safe % E;
			oct   = (safe / E) - 1000;
		}
	};

	Decode prg_prm;
	Decode prg_cv;
	Decode input;

	dsp::SchmittTrigger note_trigger[T];
	bool note_enable[E][T] = {};
	float gen[N] = {0,1,2,3,4,5};
	const char *note_text[T] = {"C","C#","D","D#","E","F","F#","G","G#","A","Bb","B","C","C#","D","D#","E","F","F#","G","G#","A","Bb","B","C"};

	//--------------------------------------------------------------------------------------------------------
	//! \brief Constructor.

	Chord_G1() {
		config(NUM_PARAMS, NUM_INPUTS, N * NUM_OUTPUTS, NUM_LIGHTS);
		configParam(NOTE_PARAM, 0.0f, 1.0f, 0.0f, "");
		configParam(PROG_PARAM, 0.0f, 12.0f, 12.0f, "Select Program");

		for (std::size_t i=0; i<T; ++i)
		{
			configParam(i + Chord_G1::NOTE_PARAM, 0.0f, 1.0f, 0.0f, note_text[i]);
		}
	}

	//--------------------------------------------------------------------------------------------------------
	//! \brief Save data.

	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		if (json_t *neJA = json_array())
		{
			for (std::size_t i = 0; i < E; ++i)
			{
				for (std::size_t j = 0; j < T; ++j)
				{
					if (json_t *neJI = json_integer((int) note_enable[i][j]))
					{
						json_array_append_new(neJA, neJI);
					}
				}
			}

			json_object_set_new(rootJ, "note_enable", neJA);
		}

		return rootJ;
	}

	//--------------------------------------------------------------------------------------------------------
	//! \brief Load data.

	void dataFromJson(json_t *rootJ) override {
		// Note enable
		if (json_t *neJA = json_object_get(rootJ, "note_enable"))
		{
			for (std::size_t i = 0; i < E; ++i)
			{
				for (std::size_t j = 0; j < T; ++j)
				{
					if (json_t *neJI = json_array_get(neJA, i*T+j))
					{
						note_enable[i][j] = !!json_integer_value(neJI);
					}
				}
			}
		}
	}

	//--------------------------------------------------------------------------------------------------------
	//! \brief Output map.

	static constexpr std::size_t omap(std::size_t port, std::size_t bank)
	{
		return port + bank * NUM_OUTPUTS;
	}

	//--------------------------------------------------------------------------------------------------------
	//! \brief Step function.

	void process(const ProcessArgs& args) override
	{
		// Clear all lights

		float leds[NUM_LIGHTS] = {};

		// Decode inputs and params

		bool act_prm = false;

		if (params[PROG_PARAM].getValue() < 12.0f)
		{
			prg_prm.step(params[PROG_PARAM].getValue() / 12.0f);
			act_prm = true;
		}

		prg_cv.step(inputs[PROG_INPUT].getVoltage());
		input .step(inputs[VOCT_INPUT].getVoltage());

		float gate = clamp(inputs[GATE_INPUT].getNormalVoltage(10.0f), 0.0f, 10.0f);

		// Input leds

		if (act_prm)
		{
			leds[PROG_LIGHT + prg_prm.key*2] = 1.0f;  // Green
		}
		else
		{
			leds[PROG_LIGHT + prg_cv.key*2+1] = 1.0f;  // Red
		}

		leds[FUND_LIGHT + input.key] = 1.0f;  // Red

		// Chord bit

		if (act_prm)
		{
			// Detect buttons and deduce what's enabled

			for (std::size_t j = 0; j < T; ++j)
			{
				if (note_trigger[j].process(params[j + NOTE_PARAM].getValue()))
				{
					note_enable[prg_prm.key][j] = !note_enable[prg_prm.key][j];
				}
			}

			for (std::size_t j = 0, b = 0; j < T; ++j)
			{
				if (note_enable[prg_prm.key][j])
				{
					if (b++ >= N)
					{
						note_enable[prg_prm.key][j] = false;
					}
				}
			}
		}

		// Based on what's enabled turn on leds

		if (act_prm)
		{
			for (std::size_t j = 0; j < T; ++j)
			{
				if (note_enable[prg_prm.key][j])
				{
					leds[NOTE_LIGHT + j*2] = 1.0f; // Green
				}
			}
		}
		else
		{
			for (std::size_t j = 0; j < T; ++j)
			{
				if (note_enable[prg_cv.key][j])
				{
					leds[NOTE_LIGHT + j*2+1] = 1.0f; // Red
				}
			}
		}

		// Based on what's enabled generate output

		{
			std::size_t i = 0;
			if (act_prm)
			{
				for (std::size_t j = 0; j < T; ++j)
				{
					if (note_enable[prg_prm.key][j])
					{
						outputs[omap(GATE_OUTPUT, i)].setVoltage(gate);
						outputs[omap(VOCT_OUTPUT, i)].setVoltage(input.out + static_cast<float>(j) / 12.0f);
						++i;
					}
				}
			}
			else
			{
				for (std::size_t j = 0; j < T; ++j)
				{
					if (note_enable[prg_cv.key][j])
					{
						outputs[omap(GATE_OUTPUT, i)].setVoltage(gate);
						outputs[omap(VOCT_OUTPUT, i)].setVoltage(input.out + static_cast<float>(j) / 12.0f);
						++i;
					}
				}
			}

			while (i < N)
			{
				outputs[omap(GATE_OUTPUT, i)].setVoltage(0.0f);
				outputs[omap(VOCT_OUTPUT, i)].setVoltage(0.0f);  // is this a good value?
				++i;
			}
		}

		// Write output in one go, seems to prevent flicker

		for (std::size_t i=0; i<NUM_LIGHTS; ++i)
		{
			lights[i].value = leds[i];
		}
	}
};


//============================================================================================================

static double x0(double shift = 0) { return 6+6*15 + shift * 66; }
static double xd(double i, double radius = 37.0, double spill = 1.65) { return (x0()               + (radius + spill * i) * GControls::dx(i, E)); }
static double yd(double i, double radius = 37.0, double spill = 1.65) { return (GControls::gy(1.5) + (radius + spill * i) * GControls::dy(i, E)); }


//============================================================================================================
//! \brief The widget.

struct GtxWidget : ModuleWidget
{
	GtxWidget(Chord_G1 *module)
	{
		setModule(module);
		box.size = Vec(18*15, 380);

		#if GTX__SAVE_SVG
		{
			PanelGen pg(asset::plugin(pluginInstance, "build/res/Chord-G1.svg"), box.size, "CHORD-G1");

			pg.nob_med_raw(x0(),   fy(-0.28), "PROGRAM");
			pg.prt_in_raw (x0(-1), fy(-0.28), "CV");
			pg.prt_in_raw (x0(+1), fy(-0.28), "SELECT");
			pg.nob_med_raw(x0(),   fy(+0.28), "BASS NOTE");
			pg.prt_in_raw (x0(-1), fy(+0.28), "V/OCT");
			pg.prt_in_raw (x0(+1), fy(+0.28), "GATE");

			pg.bus_inx(0.5,  1,    "CHORD NOTES");
			pg.bus_out(2.0,  1,    "GATE");
			pg.bus_out(2.0,  2,    "V/OCT");

			for (double i=0.0; i<T-1.0; i+=0.1)
			{
				pg.line(Vec(GControls::xd(i), GControls::yd(i)), Vec(GControls::xd(i+0.1), GControls::yd(i+0.1)), "fill:none;stroke:#7092BE;stroke-width:2");
			}

			pg.line(Vec(x0(), GControls::gy(1.5)), Vec(GControls::xd(24), GControls::yd(24)), "fill:none;stroke:#7092BE;stroke-width:4");
			pg.line(Vec(x0(), GControls::gy(1.5)), Vec(GControls::xd(16), GControls::yd(16)), "fill:none;stroke:#7092BE;stroke-width:4");
			pg.line(Vec(x0(), GControls::gy(1.5)), Vec(GControls::xd(19), GControls::yd(19)), "fill:none;stroke:#7092BE;stroke-width:4");
			pg.line(Vec(x0(), GControls::gy(1.5)), Vec(GControls::xd(22), GControls::yd(22)), "fill:none;stroke:#7092BE;stroke-width:4");
		}
		#endif

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Chord-G1.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

		addParam(createParamCentered<GControls::KnobSnapSml>(Vec(x0(+1), GControls::fy(-0.28)), module, Chord_G1::PROG_PARAM));
		addInput(createInputCentered<GControls::PortInMed>  (Vec(x0(-1), GControls::fy(-0.28)), module, Chord_G1::PROG_INPUT));
		addInput(createInputCentered<GControls::PortInMed>  (Vec(x0(+1), GControls::fy(+0.28)), module, Chord_G1::GATE_INPUT));
		addInput(createInputCentered<GControls::PortInMed>  (Vec(x0(-1), GControls::fy(+0.28)), module, Chord_G1::VOCT_INPUT));

		for (std::size_t i=0; i<N; ++i)
		{
			addOutput(createOutputCentered<GControls::PortOutMed>(Vec(GControls::px(2, i), GControls::py(1, i)), module, Chord_G1::omap(Chord_G1::GATE_OUTPUT, i)));
			addOutput(createOutputCentered<GControls::PortOutMed>(Vec(GControls::px(2, i), GControls::py(2, i)), module, Chord_G1::omap(Chord_G1::VOCT_OUTPUT, i)));
		}

		for (std::size_t i=0; i<T; ++i)
		{
			addParam(createParam<LEDButton>(GControls::but(xd(i), yd(i)), module, i + Chord_G1::NOTE_PARAM));
			addChild(createLight<MediumLight<GreenRedLight>>(GControls::l_m(xd(i), yd(i)), module, Chord_G1::NOTE_LIGHT + i*2));
		}

		addChild(createLight<SmallLight<GreenRedLight>>(GControls::l_s(x0() - 30, GControls::fy(-0.28) + 5), module, Chord_G1::PROG_LIGHT +  0*2));  // C
		addChild(createLight<SmallLight<GreenRedLight>>(GControls::l_s(x0() - 25, GControls::fy(-0.28) - 5), module, Chord_G1::PROG_LIGHT +  1*2));  // C#
		addChild(createLight<SmallLight<GreenRedLight>>(GControls::l_s(x0() - 20, GControls::fy(-0.28) + 5), module, Chord_G1::PROG_LIGHT +  2*2));  // D
		addChild(createLight<SmallLight<GreenRedLight>>(GControls::l_s(x0() - 15, GControls::fy(-0.28) - 5), module, Chord_G1::PROG_LIGHT +  3*2));  // Eb
		addChild(createLight<SmallLight<GreenRedLight>>(GControls::l_s(x0() - 10, GControls::fy(-0.28) + 5), module, Chord_G1::PROG_LIGHT +  4*2));  // E
		addChild(createLight<SmallLight<GreenRedLight>>(GControls::l_s(x0()     , GControls::fy(-0.28) + 5), module, Chord_G1::PROG_LIGHT +  5*2));  // F
		addChild(createLight<SmallLight<GreenRedLight>>(GControls::l_s(x0() +  5, GControls::fy(-0.28) - 5), module, Chord_G1::PROG_LIGHT +  6*2));  // Fs
		addChild(createLight<SmallLight<GreenRedLight>>(GControls::l_s(x0() + 10, GControls::fy(-0.28) + 5), module, Chord_G1::PROG_LIGHT +  7*2));  // G
		addChild(createLight<SmallLight<GreenRedLight>>(GControls::l_s(x0() + 15, GControls::fy(-0.28) - 5), module, Chord_G1::PROG_LIGHT +  8*2));  // Ab
		addChild(createLight<SmallLight<GreenRedLight>>(GControls::l_s(x0() + 20, GControls::fy(-0.28) + 5), module, Chord_G1::PROG_LIGHT +  9*2));  // A
		addChild(createLight<SmallLight<GreenRedLight>>(GControls::l_s(x0() + 25, GControls::fy(-0.28) - 5), module, Chord_G1::PROG_LIGHT + 10*2));  // Bb
		addChild(createLight<SmallLight<GreenRedLight>>(GControls::l_s(x0() + 30, GControls::fy(-0.28) + 5), module, Chord_G1::PROG_LIGHT + 11*2));  // B

		addChild(createLight<SmallLight<RedLight>>(GControls::l_s(x0() - 30, GControls::fy(+0.28) + 5), module, Chord_G1::FUND_LIGHT +  0));  // C
		addChild(createLight<SmallLight<RedLight>>(GControls::l_s(x0() - 25, GControls::fy(+0.28) - 5), module, Chord_G1::FUND_LIGHT +  1));  // C#
		addChild(createLight<SmallLight<RedLight>>(GControls::l_s(x0() - 20, GControls::fy(+0.28) + 5), module, Chord_G1::FUND_LIGHT +  2));  // D
		addChild(createLight<SmallLight<RedLight>>(GControls::l_s(x0() - 15, GControls::fy(+0.28) - 5), module, Chord_G1::FUND_LIGHT +  3));  // Eb
		addChild(createLight<SmallLight<RedLight>>(GControls::l_s(x0() - 10, GControls::fy(+0.28) + 5), module, Chord_G1::FUND_LIGHT +  4));  // E
		addChild(createLight<SmallLight<RedLight>>(GControls::l_s(x0()     , GControls::fy(+0.28) + 5), module, Chord_G1::FUND_LIGHT +  5));  // F
		addChild(createLight<SmallLight<RedLight>>(GControls::l_s(x0() +  5, GControls::fy(+0.28) - 5), module, Chord_G1::FUND_LIGHT +  6));  // Fs
		addChild(createLight<SmallLight<RedLight>>(GControls::l_s(x0() + 10, GControls::fy(+0.28) + 5), module, Chord_G1::FUND_LIGHT +  7));  // G
		addChild(createLight<SmallLight<RedLight>>(GControls::l_s(x0() + 15, GControls::fy(+0.28) - 5), module, Chord_G1::FUND_LIGHT +  8));  // Ab
		addChild(createLight<SmallLight<RedLight>>(GControls::l_s(x0() + 20, GControls::fy(+0.28) + 5), module, Chord_G1::FUND_LIGHT +  9));  // A
		addChild(createLight<SmallLight<RedLight>>(GControls::l_s(x0() + 25, GControls::fy(+0.28) - 5), module, Chord_G1::FUND_LIGHT + 10));  // Bb
		addChild(createLight<SmallLight<RedLight>>(GControls::l_s(x0() + 30, GControls::fy(+0.28) + 5), module, Chord_G1::FUND_LIGHT + 11));  // B
	}
};


Model *modelChord_G1 = createModel<Chord_G1, GtxWidget>("Chord-G1");
