//============================================================================================================
//!
//! \file VU-G1.cpp
//!
//! \brief VU-G1 is a simple six input volume monitoring module.
//!
//============================================================================================================

#include "Gratrix.hpp"

//============================================================================================================
//! \brief The module.

struct VU_G1 : Module
{
	enum ParamIds {
		NUM_PARAMS
	};
	enum InputIds {
		IN1_INPUT,      // N+1
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS,
	};
	enum LightIds {
		NUM_LIGHTS = 10  // N
	};

	VU_G1() {config(NUM_PARAMS, (GTX__N+1) * NUM_INPUTS, NUM_OUTPUTS, GTX__N * NUM_LIGHTS);}

	static constexpr std::size_t imap(std::size_t port, std::size_t bank)
	{
		return port + bank * NUM_INPUTS;
	}

	void process(const ProcessArgs& args) override
	{
		for (std::size_t i=0; i<GTX__N; ++i)
		{
			float input = inputs[imap(IN1_INPUT, i)].isConnected() ? inputs[imap(IN1_INPUT, i)].getVoltage() : inputs[imap(IN1_INPUT, GTX__N)].getVoltage();
			float dB    = logf(fabsf(input * 0.1f)) * (10.0f / logf(20.0f));
			float dB2   = dB * (1.0f / 3.0f);

			for (int j = 0; j < NUM_LIGHTS; j++)
			{
				float b = clamp(dB2 + (j+1), 0.0f, 1.0f);
				lights[NUM_LIGHTS * i + j].setSmoothBrightness(b * 0.9f, 10);
			}
		}
	}
};


//============================================================================================================
//! \brief The widget.

struct GtxWidget : ModuleWidget
{
	GtxWidget(VU_G1 *module)
	{
		setModule(module);
		box.size = Vec(6*15, 380);

		#if GTX__SAVE_SVG
		{
			PanelGen pg(asset::plugin(pluginInstance, "build/res/VU-G1.svg"), box.size, "VU-G1");
			pg.nob_big(0, 0, "VOLUME");
			pg.bus_in (0, 2, "IN");
		}
		#endif

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/VU-G1.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

		for (std::size_t i=0; i<GTX__N; ++i)
		{
			addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::px(0, i), GControls::py(2, i)), module, VU_G1::imap(VU_G1::IN1_INPUT, i)));
		}

		addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::gx(0), GControls::gy(2)), module, VU_G1::imap(VU_G1::IN1_INPUT, GTX__N)));

		for (std::size_t i=0, k=0; i<GTX__N; ++i)
		{
			for (std::size_t j=0; j<VU_G1::NUM_LIGHTS; ++j, ++k)
			{
				switch (j)
				{
					case 0  : addChild(createLight<SmallLight<   RedLight>>(GControls::l_s(GControls::gx(0)+(i-2.5f)*13.0f, GControls::gy(0.5)+(j-4.5f)*11.0f), module, k)); break;
					case 1  :
					case 2  : addChild(createLight<SmallLight<YellowLight>>(GControls::l_s(GControls::gx(0)+(i-2.5f)*13.0f, GControls::gy(0.5)+(j-4.5f)*11.0f), module, k)); break;
					default : addChild(createLight<SmallLight< GreenLight>>(GControls::l_s(GControls::gx(0)+(i-2.5f)*13.0f, GControls::gy(0.5)+(j-4.5f)*11.0f), module, k)); break;
				}
			}
		}
	}
};


Model *modelVU_G1 = createModel<VU_G1, GtxWidget>("VU-G1");
