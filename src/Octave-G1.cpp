//============================================================================================================
//!
//! \file Octave-G1.cpp
//!
//! \brief Octave-G1 quantises the input to 12-ET and provides an octaves-worth of output.
//!
//============================================================================================================

#include "Gratrix.hpp"

//============================================================================================================
//! \brief Some settings.

enum Spec
{
	LO_BEGIN = -5,    // C-1
	LO_END   =  5,    // C+9
	LO_SIZE  = LO_END - LO_BEGIN + 1,
	E        = 12,    // ET
	N        = 12,    // Number of note outputs
	T        = 2,
	M        = 2*T+1  // Number of octave outputs
};


//============================================================================================================
//! \brief The module.

struct Octave_G1 : Module
{
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		VOCT_INPUT   = 0,
		NUM_INPUTS = VOCT_INPUT + 1
	};
	enum OutputIds {
		NOTE_OUTPUT = 0,
		OCT_OUTPUT  = NOTE_OUTPUT + N,
		NUM_OUTPUTS = OCT_OUTPUT  + M
	};
	enum LightIds {
		KEY_LIGHT  = 0,
		OCT_LIGHT  = KEY_LIGHT + E,
		NUM_LIGHTS = OCT_LIGHT + LO_SIZE
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

	Decode input;

	//--------------------------------------------------------------------------------------------------------
	//! \brief Constructor.

	Octave_G1() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
	}

	//--------------------------------------------------------------------------------------------------------
	//! \brief Step function.

	void process(const ProcessArgs& args) override
	{
		// Clear all lights

		float leds[NUM_LIGHTS] = {};

		// Decode inputs and params
		input.step(inputs[VOCT_INPUT].getVoltage());

		for (std::size_t i=0; i<N; ++i)
		{
			outputs[i + NOTE_OUTPUT].setVoltage(input.out + i * input.s);
		}

		for (std::size_t i=0; i<M; ++i)
		{
			outputs[i + OCT_OUTPUT].setVoltage((input.out - T) + i);
		}

		// Lights

		leds[KEY_LIGHT + input.key] = 1.0f;

		if (LO_BEGIN <= input.oct && input.oct <= LO_END)
		{
			leds[OCT_LIGHT + input.oct - LO_BEGIN] = 1.0f;
		}

		// Write output in one go, seems to prevent flicker

		for (std::size_t i=0; i<NUM_LIGHTS; ++i)
		{
			lights[i].value = leds[i];
		}
	}
};


static int x(std::size_t i, double radius) { return static_cast<int>(6*15     + 0.5 + radius * GControls::dx(i, E)); }
static int y(std::size_t i, double radius) { return static_cast<int>(-20+206  + 0.5 + radius * GControls::dy(i, E)); }


//============================================================================================================
//! \brief The widget.

struct GtxWidget : ModuleWidget
{
	GtxWidget(Octave_G1 *module)
	{
		setModule(module);
		box.size = Vec(12*15, 380);

	//	double r1 = 30;
		double r2 = 55;

		#if GTX__SAVE_SVG
		{
			PanelGen pg(asset::plugin(pluginInstance, "build/res/Octave-G1.svg"), box.size, "OCTAVE-G1");

			pg.circle(Vec(x(0, 0), y(0, 0)), r2+16, "fill:#7092BE;stroke:none");
			pg.circle(Vec(x(0, 0), y(0, 0)), r2-16, "fill:#CEE1FD;stroke:none");

	/*		// Wires
			for (std::size_t i=0; i<N; ++i)
			{
				pg.line(Vec(x(i,   r1), y(i,   r1)), Vec(x(i, r2), y(i, r2)), "stroke:#440022;stroke-width:1");
				if (i) { pg.line(Vec(x(i-1, r1), y(i-1, r1)), Vec(x(i, r1), y(i, r1)), "stroke:#440022;stroke-width:1"); }
			}
	*/
			// Ports
			pg.circle(Vec(x(0, 0), y(0, 0)), 10, "stroke:#440022;stroke-width:1");
			for (std::size_t i=0; i<N; ++i)
			{
				 pg.circle(Vec(x(i, r2), y(i,   r2)), 10, "stroke:#440022;stroke-width:1");
			}

			pg.prt_out(-0.20, 2,          "", "-2");
			pg.prt_out( 0.15, 2,          "", "-1");
			pg.prt_out( 0.50, 2, "TRANSPOSE",  "0");
			pg.prt_out( 0.85, 2,          "", "+1");
			pg.prt_out( 1.20, 2,          "", "+2");
		}
		#endif

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Octave-G1.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

		addInput(createInputCentered<GControls::PortInMed>(Vec(x(0, 0), y(0, 0)), module, Octave_G1::VOCT_INPUT));
		for (std::size_t i=0; i<N; ++i)
		{
			addOutput(createOutputCentered<GControls::PortOutMed>(Vec(x(i, r2), y(i, r2)), module, i + Octave_G1::NOTE_OUTPUT));
		}

		addOutput(createOutputCentered<GControls::PortOutMed>(Vec(GControls::gx(-0.20), GControls::gy(2)), module, 0 + Octave_G1::OCT_OUTPUT));
		addOutput(createOutputCentered<GControls::PortOutMed>(Vec(GControls::gx( 0.15), GControls::gy(2)), module, 1 + Octave_G1::OCT_OUTPUT));
		addOutput(createOutputCentered<GControls::PortOutMed>(Vec(GControls::gx( 0.50), GControls::gy(2)), module, 2 + Octave_G1::OCT_OUTPUT));
		addOutput(createOutputCentered<GControls::PortOutMed>(Vec(GControls::gx( 0.85), GControls::gy(2)), module, 3 + Octave_G1::OCT_OUTPUT));
		addOutput(createOutputCentered<GControls::PortOutMed>(Vec(GControls::gx( 1.20), GControls::gy(2)), module, 4 + Octave_G1::OCT_OUTPUT));

		addChild(createLight<SmallLight<RedLight>>(GControls::l_s(GControls::gx(0.5) - 30, GControls::fy(0-0.28) + 5), module, Octave_G1::KEY_LIGHT +  0));  // C
		addChild(createLight<SmallLight<RedLight>>(GControls::l_s(GControls::gx(0.5) - 25, GControls::fy(0-0.28) - 5), module, Octave_G1::KEY_LIGHT +  1));  // C#
		addChild(createLight<SmallLight<RedLight>>(GControls::l_s(GControls::gx(0.5) - 20, GControls::fy(0-0.28) + 5), module, Octave_G1::KEY_LIGHT +  2));  // D
		addChild(createLight<SmallLight<RedLight>>(GControls::l_s(GControls::gx(0.5) - 15, GControls::fy(0-0.28) - 5), module, Octave_G1::KEY_LIGHT +  3));  // Eb
		addChild(createLight<SmallLight<RedLight>>(GControls::l_s(GControls::gx(0.5) - 10, GControls::fy(0-0.28) + 5), module, Octave_G1::KEY_LIGHT +  4));  // E
		addChild(createLight<SmallLight<RedLight>>(GControls::l_s(GControls::gx(0.5)     , GControls::fy(0-0.28) + 5), module, Octave_G1::KEY_LIGHT +  5));  // F
		addChild(createLight<SmallLight<RedLight>>(GControls::l_s(GControls::gx(0.5) +  5, GControls::fy(0-0.28) - 5), module, Octave_G1::KEY_LIGHT +  6));  // Fs
		addChild(createLight<SmallLight<RedLight>>(GControls::l_s(GControls::gx(0.5) + 10, GControls::fy(0-0.28) + 5), module, Octave_G1::KEY_LIGHT +  7));  // G
		addChild(createLight<SmallLight<RedLight>>(GControls::l_s(GControls::gx(0.5) + 15, GControls::fy(0-0.28) - 5), module, Octave_G1::KEY_LIGHT +  8));  // Ab
		addChild(createLight<SmallLight<RedLight>>(GControls::l_s(GControls::gx(0.5) + 20, GControls::fy(0-0.28) + 5), module, Octave_G1::KEY_LIGHT +  9));  // A
		addChild(createLight<SmallLight<RedLight>>(GControls::l_s(GControls::gx(0.5) + 25, GControls::fy(0-0.28) - 5), module, Octave_G1::KEY_LIGHT + 10));  // Bb
		addChild(createLight<SmallLight<RedLight>>(GControls::l_s(GControls::gx(0.5) + 30, GControls::fy(0-0.28) + 5), module, Octave_G1::KEY_LIGHT + 11));  // B

		for (std::size_t i=0; i<LO_SIZE; ++i)
		{
			addChild(createLight<SmallLight<RedLight>>(GControls::l_s(GControls::gx(0.5) + (i - LO_SIZE/2) * 10, GControls::fy(0-0.28) + 20), module, Octave_G1::OCT_LIGHT + i));
		}
	}
};


Model *modelOctave_G1 = createModel<Octave_G1, GtxWidget>("Octave-G1");
