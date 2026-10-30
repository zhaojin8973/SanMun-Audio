#include "dsp/AcousticFeatureExtractor.h"
#include "ai/LlmClient.h"
#include "ai/ListenAssistant.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <iostream>
int main(int argc,char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if(argc<3) return 2;
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(juce::File(juce::String::fromUTF8(argv[1]))));
    if(!reader) return 3;
    const auto start=juce::int64(std::stod(argv[2])*reader->sampleRate);
    const auto count=int(std::min(juce::int64(reader->sampleRate*10),reader->lengthInSamples-start));
    if(count<1) return 4;
    juce::AudioBuffer<float> audio(2,count);reader->read(&audio,0,count,start,true,true);
    if(argc>3) audio.applyGain(juce::Decibels::decibelsToGain(float(std::stod(argv[3]))));
    ai::AcousticFeatureExtractor extractor;extractor.prepare(reader->sampleRate,512);extractor.startListening();
    for(int n=0;n<count;n+=512){juce::AudioBuffer<float> b(audio.getArrayOfWritePointers(),2,n,std::min(512,count-n));extractor.processBlock(b);}
    extractor.computeFeatures();const auto f=extractor.getLatestFeatures();
    ai::CompressorPresetRecommendation r;
    ai::ListenResult result;
    if (argc > 4 && std::string(argv[4]) == "features") {
        std::cout << f.toJsonString() << std::endl; return 0;
    }
    if (argc > 4) {
        ai::ListenOptions options;
        if (argc > 5) options.targetGrDb = float(std::stod(argv[5]));
        if (argc > 6) options.fixedMode = std::stoi(argv[6]);
        result = ai::ListenAssistant::analyse(extractor.copyCapture(),false,ai::AcousticSourceType::Unknown,nullptr,options); r=result.preset;
    }
    else { ai::LlmClient client;r=client.generateLocalSingleRecommendation(f).preset;extractor.calibrateRecommendationWithVirtualDsp(r); }
    // 手动目标模式报告助理实际使用的特征快照，避免测试工具另行分类后造成误解。
    const auto& diagnostics = argc > 4 ? result.features : f;
    auto v=juce::JSON::parse(diagnostics.toJsonString());auto* o=v.getDynamicObject();
    o->setProperty("file",juce::File(juce::String::fromUTF8(argv[1])).getFileName());o->setProperty("start",std::stod(argv[2]));
    o->setProperty("mode",r.mode);o->setProperty("comp",r.voxComp);o->setProperty("drive",r.fetInput);o->setProperty("pr",r.optoPR);
    o->setProperty("level_error",r.calibratedRmsDb-r.calibrationInputRmsDb);o->setProperty("matched",r.calibrationWithinRange);
    o->setProperty("target_gr",r.requestedGrDb);o->setProperty("gr",r.measuredGrDb);o->setProperty("max_gr",r.maximumGrDb);o->setProperty("ready",r.usable);
    juce::Array<juce::var> cs;
    for (const auto& c : result.candidates) { juce::DynamicObject::Ptr d=new juce::DynamicObject();
      d->setProperty("mode",c.mode);d->setProperty("gr",c.measuredGrDb);d->setProperty("target",c.requestedGrDb);
      d->setProperty("max_gr",c.maximumGrDb);d->setProperty("delta",c.calibratedRmsDb-c.calibrationInputRmsDb);d->setProperty("usable",c.usable);cs.add(juce::var(d)); }
    o->setProperty("candidates",cs);
    std::cout<<juce::JSON::toString(v,true)<<std::endl;
}
