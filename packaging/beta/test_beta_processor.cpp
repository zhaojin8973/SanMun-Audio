#include "PluginProcessor.h"
#include <iostream>
#include <stdexcept>
using Processor = SANMU_BETA_PROCESSOR;
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("sanmu-beta-processor","",false);
    dir.createDirectory();
    auto record=dir.getChildFile("clock"); record.replaceWithText(juce::String(sanmu_beta::deadline));
    for(int channels : {1,2})
    {
        Processor processor(record);
        check(!processor.betaAccess.allowed(),"到期原生处理器不得运行效果");
        auto layout=processor.getBusesLayout();
        layout.inputBuses.set(0,channels==1?juce::AudioChannelSet::mono():juce::AudioChannelSet::stereo());
        layout.outputBuses.set(0,layout.inputBuses[0]);
        check(processor.setBusesLayout(layout),"单声道与立体声布局");
        processor.setRateAndBufferSizeDetails(48000,128); processor.prepareToPlay(48000,128);
        juce::MemoryBlock before,after;processor.getStateInformation(before);
        std::vector<float> parameters; for(auto* p:processor.getParameters())parameters.push_back(p->getValue());
        const int latency=processor.getLatencySamples();
        juce::AudioBuffer<float> block(channels,128); juce::MidiBuffer midi;
        int sample=0;
        for(int k=0;k<(latency+512)/128+1;++k)
        {
            block.clear(); if(sample==0)for(int c=0;c<channels;++c)block.setSample(c,0,.25f);
            processor.processBlock(block,midi);
            for(int c=0;c<channels;++c)for(int n=0;n<128;++n)
                check(block.getSample(c,n)==(sample+n==latency?.25f:0.f),"到期后输出必须是延迟对齐的原声");
            sample+=128;
        }
        processor.getStateInformation(after); processor.setStateInformation(after.getData(),int(after.getSize()));
        int index=0;for(auto* p:processor.getParameters())check(p->getValue()==parameters[size_t(index++)],"到期与状态恢复不得改变参数");
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        check(editor!=nullptr,"到期后编辑器仍可打开");
        bool notice=false;
        for(auto* child:editor->getChildren()) if(dynamic_cast<sanmu_beta::Notice*>(child))
        { notice=true; check(child->getHeight()==28 && child->isVisible(),"期限提示条布局"); }
        check(notice,"原生期限提示条存在");
        processor.releaseResources();
    }
    dir.deleteRecursively();
    std::cout<<"原生处理器到期直通、单双声道、参数恢复、编辑器与提示条验证通过\n";
}
