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
		void step(bool external, int frameCount, const Param &trig_param, const Input &x_input, const Input &trig_input);
	};

	bool external = false;
	Voice voice[GTX__N+1];

	Scope() {
		config(NUM_PARAMS, GTX__N * NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(X_SCALE_PARAM, -2.0f,      8.0f,   0.0f, "");
		configParam(X_POS_PARAM,  -10.0f,     10.0f,   0.0f, "");
		configParam(TIME_PARAM,    -6.0f,    -16.0f, -14.0f, "");
		configParam(TRIG_PARAM,   -10.0f,     10.0f,   0.0f, "");
		configParam(EXTERNAL_PARAM, 0.0f,      1.0f,   1.0f, "");
		configParam(DISP_PARAM,     0.0f,  GTX__N+1,   0.0f, "");
	}

	static constexpr std::size_t imap(std::size_t port, std::size_t bank)
	{
		return port + bank * NUM_INPUTS;
	}

	void step() override;
};


void Scope::step() {
	// Modes
	external = params[EXTERNAL_PARAM].getValue() <= 0.0f;

	// Compute time
	float deltaTime = powf(2.0f, params[TIME_PARAM].getValue());
	int frameCount = (int)ceilf(deltaTime * APP->engine->getSampleRate());

	Input x_sum, trig_sum;
	int count = 0;

	for (int k=0; k<GTX__N; ++k)
	{
		voice[k].step(external, frameCount, params[TRIG_PARAM], inputs[imap(X_INPUT, k)], inputs[imap(TRIG_INPUT, k)]);

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

	voice[GTX__N].step(external, frameCount, params[TRIG_PARAM], x_sum, trig_sum);
}

void Scope::Voice::step(bool external, int frameCount, const Param &trig_param, const Input &x_input, const Input &trig_input) {
	// Copy active state
	active = x_input.active;

	// Add frame to buffer
	if (bufferIndex < BUFFER_SIZE) {
		if (++frameIndex > frameCount) {
			frameIndex = 0;
			bufferX[bufferIndex] = x_input.value;
			bufferIndex++;
		}
	}

	// Are we waiting on the next trigger?
	if (bufferIndex >= BUFFER_SIZE) {
		// Trigger immediately if external but nothing plugged in
		if (external && !trig_input.active) {
			bufferIndex = 0;
			frameIndex = 0;
			return;
		}

		// Reset the Schmitt trigger so we don't trigger immediately if the input is high
		if (frameIndex == 0) {
			resetTrigger.reset();
		}
		frameIndex++;

		// Must go below 0.1fV to trigger
		float gate = external ? trig_input.value : x_input.value;

		// Reset if triggered
		float holdTime = 0.1f;
		if (resetTrigger.process(rescale(gate, trig_param.value - 0.1f, trig_param.value, 0.f, 1.f)) || (frameIndex >= APP->engine->getSampleRate() * holdTime)) {
			bufferIndex = 0; frameIndex = 0; return;
		}

		// Reset if we've waited too long
		if (frameIndex >= APP->engine->getSampleRate() * holdTime) {
			bufferIndex = 0; frameIndex = 0; return;
		}
	}
}


struct Display : TransparentWidget {
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

	Display() {
		font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Sudo.ttf"));
	}

	void drawWaveform(NVGcontext *ctx, float *valuesX, const Rect &b) {
		if (!valuesX)
			return;
		nvgSave(ctx);
		nvgScissor(ctx, b.pos.x, b.pos.y, b.size.x, b.size.y);
		nvgBeginPath(ctx);
		// Draw maximum display left to right
		for (int i = 0; i < BUFFER_SIZE; i++) {
			float x = (float)i / (BUFFER_SIZE - 1);
			float y = valuesX[i] / 2.0f + 0.5f;
			Vec p;
			p.x = b.pos.x + b.size.x * x;
			p.y = b.pos.y + b.size.y * (1.0f - y);
			if (i == 0)
				nvgMoveTo(ctx, p.x, p.y);
			else
				nvgLineTo(ctx, p.x, p.y);
		}
		nvgLineCap(ctx, NVG_ROUND);
		nvgMiterLimit(ctx, 2.0);
		nvgStrokeWidth(ctx, 1.5);
		nvgGlobalCompositeOperation(ctx, NVG_LIGHTER);
		nvgStroke(ctx);
		nvgResetScissor(ctx);
		nvgRestore(ctx);
	}

	void drawTrig(NVGcontext *ctx, float value, const Rect &b) {
		nvgScissor(ctx, b.pos.x, b.pos.y, b.size.x, b.size.y);
		value = value / 2.0f + 0.5f;
		Vec p = Vec(b.pos.x + b.size.x, b.pos.y + b.size.y * (1.0f - value));

		// Draw line
		nvgStrokeColor(ctx, nvgRGBA(0xff, 0xff, 0xff, 0x10));
		{
			nvgBeginPath(ctx);
			nvgMoveTo(ctx, p.x - 13, p.y);
			nvgLineTo(ctx, 0, p.y);
			nvgClosePath(ctx);
		}
		nvgStroke(ctx);

		// Draw indicator
		nvgFillColor(ctx, nvgRGBA(0xff, 0xff, 0xff, 0x60));
		{
			nvgBeginPath(ctx);
			nvgMoveTo(ctx, p.x - 2, p.y - 4);
			nvgLineTo(ctx, p.x - 9, p.y - 4);
			nvgLineTo(ctx, p.x - 13, p.y);
			nvgLineTo(ctx, p.x - 9, p.y + 4);
			nvgLineTo(ctx, p.x - 2, p.y + 4);
			nvgClosePath(ctx);
		}
		nvgFill(ctx);

		nvgFontSize(ctx, 9);
		nvgFontFaceId(ctx, font->handle);
		nvgFillColor(ctx, nvgRGBA(0x1e, 0x28, 0x2b, 0xff));
		nvgText(ctx, p.x - 8, p.y + 3, "T", NULL);
		nvgResetScissor(ctx);
	}

	void drawStats(NVGcontext *ctx, Vec pos, const char *title, const Stats &stats) {
		nvgFontSize(ctx, 13);
		nvgFontFaceId(ctx, font->handle);
		nvgTextLetterSpacing(ctx, -2);

		nvgFillColor(ctx, nvgRGBA(0xff, 0xff, 0xff, 0x80));
		char text[128];
		snprintf(text, sizeof(text), "%s. %4.1f [%+5.1f %+5.1f]", title, stats.vpp, stats.vmin, stats.vmax);
		nvgText(ctx, pos.x + 6, pos.y + 11, text, NULL);
	}

	void draw(const DrawArgs& args) override {
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
				drawWaveform(args.vg, valuesX, b);
			}

			float valueTrig = (module->params[Scope::TRIG_PARAM].getValue() + offsetX) * gainX / 10.0;
			drawTrig(args.vg, valueTrig, b);

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
			drawStats(args.vg, stats_pos, stats_lab[k], statsX[k]);
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

			pg.nob_sml_raw(GControls::gx(1-0.22), GControls::gy(2-0.24), "SCALE");
			pg.nob_sml_raw(GControls::gx(1-0.22), GControls::gy(2+0.22), "POS");
			pg.nob_sml_raw(GControls::gx(1+0.22), GControls::gy(2-0.24), "TIME");
			pg.nob_sml_raw(GControls::gx(1+0.22), GControls::gy(2+0.22), "TRIG");
			pg.tog_raw    (GControls::gx(3-0.22), GControls::gy(2-0.24), "INT", "EXT");
			pg.nob_sml_raw(GControls::gx(3+0.22), GControls::gy(2-0.24), "DISP");

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

		{
			Display *display  = new Display();
			display->module   = module;
			display->box.pos  = screen_pos;
			display->box.size = screen_size;
			addChild(display);
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
