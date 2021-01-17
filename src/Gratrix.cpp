#include "Gratrix.hpp"

Plugin *pluginInstance;

void init(rack::Plugin *p)
{
	pluginInstance = p;

	p->addModel(modelADSR_F1);
	p->addModel(modelBinary_G1);
	p->addModel(modelBlank_03);
	p->addModel(modelBlank_06);
	p->addModel(modelBlank_09);
	p->addModel(modelBlank_12);
	p->addModel(modelChord_G1);
	p->addModel(modelFade_G1);
	p->addModel(modelFade_G2);
	p->addModel(modelKeys_G1);
	p->addModel(modelOctave_G1);
	p->addModel(modelSeq_G1);
	p->addModel(modelSeq_G2);
	p->addModel(modelScope_G1);
	p->addModel(modelVCA_F1);
	p->addModel(modelVCF_F1);
	p->addModel(modelVCO_F1);
	p->addModel(modelVCO_F2);
	p->addModel(modelVU_G1);
}
