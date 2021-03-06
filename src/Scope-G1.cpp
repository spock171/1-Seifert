//============================================================================================================
//!
//! \file Scope-G1.cpp
//!
//! \brief Scope-G1 is a...
//!
//============================================================================================================


#include <string.h>
#include "Gratrix.hpp"

#define BUFFER_SIZE 512

struct Scope : Module {
	enum ParamIds {
		X_SCALE_PARAM,
		X_POS_PARAM,
		TIME_PARAM,
		TRIG_PARAM,
		EXTERNAL_PARAM,
		DISP_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		X_INPUT,     // N
		TRIG_INPUT,  // N
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	struct Voice {
		bool active = false;
		float bufferX[BUFFER_SIZE] = {};
		int bufferIndex = 0;
		float frameIndex = 0;
		dsp::SchmittTrigger resetTrigger;
		void step(bool external, int frameCount, const Param &trig_param, const Input &x_input, const Input &trig_input, float s_Rate);
	};

	bool external = false;
	Voice voice[GTX__N+1];

	Scope() {
		config(NUM_PARAMS, GTX__N * NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		const float timeBase = (float) BUFFER_SIZE / 6;
		configParam(X_SCALE_PARAM, -2.0f,      8.0f,   0.0f, "X scale", " V/div", 1 / 2.f, 5);
		configParam(X_POS_PARAM,  -10.0f,     10.0f,   0.0f, "X position", " V");
		configParam(TIME_PARAM,    -6.0f,    -16.0f, -14.0f, "Time", " ms/div", 1 / 2.f, 1000 * timeBase);
		configParam(TRIG_PARAM,   -10.0f,     10.0f,   0.0f, "Trigger position", " V");
		configParam(EXTERNAL_PARAM, 0.0f,      1.0f,   1.0f);
		configParam(DISP_PARAM,     0.0f,  GTX__N+1,   0.0f);
	}

	static constexpr std::size_t imap(std::size_t port, std::size_t bank)
	{
		return port + bank * NUM_INPUTS;
	}

	void process(const ProcessArgs& args) override
	{
		// Modes
		external = params[EXTERNAL_PARAM].getValue() <= 0.0f;

		// Compute time
		float deltaTime = powf(2.0f, params[TIME_PARAM].getValue());
		float sample_Rate = args.sampleRate;
		int frameCount = (int)ceilf(deltaTime * sample_Rate);

		Input x_sum, trig_sum;
		int count = 0;

		for (int k=0; k<GTX__N; ++k)
		{
			voice[k].step(external, frameCount, params[TRIG_PARAM], inputs[imap(X_INPUT, k)], inputs[imap(TRIG_INPUT, k)], sample_Rate);
	
			if (inputs[imap(X_INPUT, k)].isConnected())
			{
				x_sum.active = true;
				x_sum.value += inputs[imap(X_INPUT, k)].getVoltage();
				count++;
			}

			if (inputs[imap(TRIG_INPUT, k)].isConnected()) // GTX TODO - may need better logic here
			{
				trig_sum.active = true;
				trig_sum.value += inputs[imap(TRIG_INPUT, k)].getVoltage();
			}
		}

		if (count > 0)
		{
			x_sum.value /= static_cast<float>(count);
		}
	
		voice[GTX__N].step(external, frameCount, params[TRIG_PARAM], x_sum, trig_sum, sample_Rate);
	}
};

void Scope::Voice::step(bool external, int frameCount, const Param &trig_param, const Input &x_input, const Input &trig_input, float s_Rate)
{
	// Copy active state
	active = x_input.active;

	// Add frame to buffer
	if (bufferIndex < BUFFER_SIZE)
	{
		if (++frameIndex > frameCount)
		{
			frameIndex = 0;
			bufferX[bufferIndex] = x_input.value;
			bufferIndex++;
		}
	}

	// Are we waiting on the next trigger?
	if (bufferIndex >= BUFFER_SIZE)
	{
		// Trigger immediately if external but nothing plugged in
		if (external && !trig_input.active)
		{
			bufferIndex = 0;
			frameIndex = 0;
			return;
		}

		// Reset the Schmitt trigger so we don't trigger immediately if the input is high
		if (frameIndex == 0)
		{
			resetTrigger.reset();
		}
		frameIndex++;

		// Must go below 0.1fV to trigger
		float gate = external ? trig_input.value : x_input.value;

		// Reset if triggered
		float holdTime = 0.1f;
		if (resetTrigger.process(rescale(gate, trig_param.value - 0.1f, trig_param.value, 0.f, 1.f)) || (frameIndex >= s_Rate * holdTime))
		{
			bufferIndex = 0; frameIndex = 0; return;
		}

		// Reset if we've waited too long
		if (frameIndex >= s_Rate * holdTime)
		{
			bufferIndex = 0; frameIndex = 0; return;
		}
	}
}


struct Display_Scope : TransparentWidget {
	Scope *module;
	int frame = 0;
	std::shared_ptr<Font> font;

	struct Stats {
		float vrms, vpp, vmin, vmax;
		void calculate(float *values) {
			vrms = 0.0f;
			vmax = -INFINITY;
			vmin = INFINITY;
			for (int i = 0; i < BUFFER_SIZE; i++) {
				float v = values[i];
				vrms += v*v;
				vmax = fmaxf(vmax, v);
				vmin = fminf(vmin, v);
			}
			vrms = sqrtf(vrms / BUFFER_SIZE);
			vpp = vmax - vmin;
		}
	};
	Stats statsX[GTX__N + 1];

	Display_Scope() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Sudo.ttf"));
	}

	void drawWaveform(const DrawArgs& args, float *valuesX, const Rect &b) {
		if (!valuesX)
			return;
		nvgSave(args.vg);
		nvgScissor(args.vg, b.pos.x, b.pos.y, b.size.x, b.size.y);
		nvgBeginPath(args.vg);
		// Draw maximum display left to right
		for (int i = 0; i < BUFFER_SIZE; i++) {
			float x = (float)i / (BUFFER_SIZE - 1);
			float y = valuesX[i] / 2.0f + 0.5f;
			Vec p;
			p.x = b.pos.x + b.size.x * x;
			p.y = b.pos.y + b.size.y * (1.0f - y);
			if (i == 0)
				nvgMoveTo(args.vg, p.x, p.y);
			else
				nvgLineTo(args.vg, p.x, p.y);
		}
		nvgLineCap(args.vg, NVG_ROUND);
		nvgMiterLimit(args.vg, 2.0);
		nvgStrokeWidth(args.vg, 1.5);
		nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
		nvgStroke(args.vg);
		nvgResetScissor(args.vg);
		nvgRestore(args.vg);
	}

	void drawTrig(const DrawArgs& args, float value, const Rect &b) {
		nvgScissor(args.vg, b.pos.x, b.pos.y, b.size.x, b.size.y);
		value = value / 2.0f + 0.5f;
		Vec p = Vec(b.pos.x + b.size.x, b.pos.y + b.size.y * (1.0f - value));

		// Draw line
		nvgStrokeColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0x10));
		{
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, p.x - 13, p.y);
			nvgLineTo(args.vg, 0, p.y);
			nvgClosePath(args.vg);
		}
		nvgStroke(args.vg);

		// Draw indicator
		nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0x60));
		{
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, p.x - 2, p.y - 4);
			nvgLineTo(args.vg, p.x - 9, p.y - 4);
			nvgLineTo(args.vg, p.x - 13, p.y);
			nvgLineTo(args.vg, p.x - 9, p.y + 4);
			nvgLineTo(args.vg, p.x - 2, p.y + 4);
			nvgClosePath(args.vg);
		}
		nvgFill(args.vg);

		nvgFontSize(args.vg, 9);
		nvgFontFaceId(args.vg, font->handle);
		nvgFillColor(args.vg, nvgRGBA(0x1e, 0x28, 0x2b, 0xff));
		nvgText(args.vg, p.x - 8, p.y + 3, "T", NULL);
		nvgResetScissor(args.vg);
	}

	void drawStats(const DrawArgs& args, Vec pos, const char *title, const Stats &stats) {
		nvgFontSize(args.vg, 13);
		nvgFontFaceId(args.vg, font->handle);
		nvgTextLetterSpacing(args.vg, -2);

		nvgFillColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0x80));
		char text[128];
		snprintf(text, sizeof(text), "%s. %4.1f [%+5.1f %+5.1f]", title, stats.vpp, stats.vmin, stats.vmax);
		nvgText(args.vg, pos.x + 6, pos.y + 11, text, NULL);
	}

	void draw(const DrawArgs& args) override {
		if (!module)
			return;
		float gainX = powf(2.0, roundf(module->params[Scope::X_SCALE_PARAM].getValue()));
		float offsetX = module->params[Scope::X_POS_PARAM].getValue();
		int   disp = static_cast<int>(module->params[Scope::DISP_PARAM].getValue() + 0.5f);

		int k0, k1, kM;

		     if (disp == 0       ) { k0 =      0; k1 = GTX__N;   kM = 0;   } // six up
		else if (disp == GTX__N+1) { k0 = GTX__N; k1 = GTX__N+1; kM = 100; } // sum
		else                       { k0 = disp-1; k1 =   disp  ; kM = 100; } // individual

		static const char *stats_lab[GTX__N+1] = {"1", "2", "3", "4", "5", "6", "SUM"};

		for (int k=k0; k<k1; ++k)
		{
			Rect a = Rect(Vec(0, 15), box.size.minus(Vec(0, 15*2)));
			Rect b = a;

			// GTX TODO - imporve
			if (kM < GTX__N)
			{
				b.size.x /= (GTX__N/2);
				b.size.y /= (GTX__N/3);
				b.pos.x  += (k%3) * b.size.x;
				b.pos.y  += (k/3) * b.size.y;
			}

			float valuesX[BUFFER_SIZE];
			for (int i = 0; i < BUFFER_SIZE; i++) {
				valuesX[i] = (module->voice[k].bufferX[i] + offsetX) * gainX / 10.0;
			}

			// Draw waveforms
			if (module->voice[k].active) {
				if (k&1) nvgStrokeColor(args.vg, nvgRGBA(0xe1, 0x02, 0x78, 0xc0));
				else     nvgStrokeColor(args.vg, nvgRGBA(0x28, 0xb0, 0xf3, 0xc0));
				drawWaveform(args, valuesX, b);
			}

			float valueTrig = (module->params[Scope::TRIG_PARAM].getValue() + offsetX) * gainX / 10.0;
			drawTrig(args, valueTrig, b);

			// Calculate and draw stats
			if (frame == 0) {
				statsX[k].calculate(module->voice[k].bufferX);
			}

			Vec stats_pos = b.pos;
			if (k >= 3 && k < GTX__N)
			{
				stats_pos.y += b.size.y;
			}
			else
			{
				stats_pos.y -= 15;
			}
			drawStats(args, stats_pos, stats_lab[k], statsX[k]);
		}

		if (++frame >= 4)
		{
			frame = 0;
		}
	}
};


//============================================================================================================
//! \brief The widget.

struct GtxWidget : ModuleWidget
{
	GtxWidget(Scope *module)
	{
		setModule(module);
		box.size = Vec(24*15, 380);

		auto screen_pos  = Vec(0, 35);
		auto screen_size = Vec(box.size.x, 220);

		#if GTX__SAVE_SVG
		{
			PanelGen pg(asset::plugin(pluginInstance, "build/res/Scope-G1.svg"), box.size, "SCOPE-G1");

			pg.rect(screen_pos, screen_size, "fill:#222222;stroke:none");

			pg.bus_in(0, 2, "IN");
			pg.bus_in(2, 2, "EXT TRIG");

			pg.nob_sml_raw(gx(1-0.22), gy(2-0.24), "SCALE");
			pg.nob_sml_raw(gx(1-0.22), gy(2+0.22), "POS");
			pg.nob_sml_raw(gx(1+0.22), gy(2-0.24), "TIME");
			pg.nob_sml_raw(gx(1+0.22), gy(2+0.22), "TRIG");
			pg.tog_raw    (gx(3-0.22), gy(2-0.24), "INT", "EXT");
			pg.nob_sml_raw(gx(3+0.22), gy(2-0.24), "DISP");

			for (std::size_t i=0; i<=12; i++)
			{
				float x = screen_pos.x + screen_size.x * i / 12.0f;
				pg.line(Vec(x, screen_pos.y + 15), Vec(x, screen_pos.y + screen_size.y - 15), "fill:none;stroke:#666666;stroke-width:1");
			}

			for (std::size_t i=0; i<=8; i++)
			{
				float y = screen_pos.y + 15 + (screen_size.y - 30) * i /  8.0f;
				pg.line(Vec(screen_pos.x, y), Vec(screen_pos.x + screen_size.x, y), "fill:none;stroke:#666666;stroke-width:1");
			}
		}
		#endif

		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Scope-G1.svg")));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

		{
			Display_Scope *display_Scope  = new Display_Scope();
			display_Scope->module   = module;
			display_Scope->box.pos  = screen_pos;
			display_Scope->box.size = screen_size;
			addChild(display_Scope);
		}

		addParam(createParamCentered<GControls::KnobSnapSml>(Vec(GControls::gx(1-0.22), GControls::gy(2-0.24)), module, Scope::X_SCALE_PARAM));
		addParam(createParamCentered<GControls::KnobFreeSml>(Vec(GControls::gx(1-0.22), GControls::gy(2+0.22)), module, Scope::X_POS_PARAM));
		addParam(createParamCentered<GControls::KnobFreeSml>(Vec(GControls::gx(1+0.22), GControls::gy(2-0.24)), module, Scope::TIME_PARAM));
		addParam(createParamCentered<GControls::KnobFreeSml>(Vec(GControls::gx(1+0.22), GControls::gy(2+0.22)), module, Scope::TRIG_PARAM));
		addParam(createParam<CKSS>  (GControls::tog(GControls::gx(3-0.22), GControls::gy(2-0.24)), module, Scope::EXTERNAL_PARAM));
		addParam(createParamCentered<GControls::KnobSnapSml>(Vec(GControls::gx(3+0.22), GControls::gy(2-0.24)), module, Scope::DISP_PARAM));

		for (std::size_t i=0; i<GTX__N; ++i)
		{
			addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::px(0, i), GControls::py(2, i)), module, Scope::imap(Scope::X_INPUT,    i)));
			addInput(createInputCentered<GControls::PortInMed>(Vec(GControls::px(2, i), GControls::py(2, i)), module, Scope::imap(Scope::TRIG_INPUT, i)));
		}
	}
};


Model *modelScope_G1 = createModel<Scope, GtxWidget>("Scope-G1");
