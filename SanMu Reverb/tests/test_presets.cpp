#include "../source/PluginProcessor.h"
#include "../source/PluginEditor.h"
#include "../source/presets/PresetManager.h"
#include "../source/presets/HallPresets.h"
#include "../source/presets/PlatePresets.h"
#include "../source/params/StateCompatibility.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <set>
#include <cstdio>
#include <string>

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    int failures=0;
    const auto check=[&](bool pass,const char* message) {
        std::printf("%s：%s\n",pass?"通过":"失败",message);if(!pass)++failures;
    };
    SanMuReverbAudioProcessor processor;
    std::set<juce::String> names;
    juce::Array<juce::var> exports;
    for(int i=0;i<PresetManager::getNumFactoryPresets();++i)
    {
        const auto& preset=PresetManager::getFactoryPreset(i);
        const bool unique=names.insert(preset.name).second;
        // 当前依赖的UTF-8子串查找混用了字符数和字节数；按完整UTF-8字节串核对历史标签。
        const bool historical=preset.name.toStdString().find("历史")!=std::string::npos;
        const bool metadata=unique&&!preset.name.isEmpty()&&!preset.purpose.isEmpty()
            &&preset.values.size()==17&&!historical&&!preset.name.contains("090")
            &&!preset.name.contains("P1")&&!preset.name.contains("P2")&&!preset.name.contains("P3")
            &&!preset.name.contains("C1")&&!preset.name.contains("R1");
        bool valid=metadata;
        for(const auto& [id,value]:preset.values)
        {
            const auto* parameter=processor.apvts.getParameter(id);
            if(parameter==nullptr){valid=false;continue;}
            const auto range=parameter->getNormalisableRange();
            valid &= std::isfinite(value)&&value>=range.start&&value<=range.end;
        }
        PresetManager::applyPreset(preset,processor.apvts);
        for(const auto& [id,value]:preset.values)
            valid &= std::abs(processor.apvts.getRawParameterValue(id)->load()-value)<.01f;
        check(valid,"预设名称唯一、参数完整有效、真实插件回读正确");
        auto* item=new juce::DynamicObject();item->setProperty("name",preset.name);
        item->setProperty("purpose",preset.purpose);
        auto* values=new juce::DynamicObject();
        for(const auto& [id,value]:preset.values) values->setProperty(id,value);
        item->setProperty("values",juce::var(values));exports.add(juce::var(item));
    }
    const auto directory=juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getNonexistentChildFile("sanmu-preset-test","",false);
    check(PresetManager::getNumFactoryPresets()==23,"工厂菜单只含23项当前正式预设");
    const std::array<int,4> expectedCounts{4,4,9,6};std::array<int,4> actualCounts{};
    int previousCategory=-1,previousMode=-1;bool grouped=true;
    for(int i=0;i<PresetManager::getNumFactoryPresets();++i)
    {
        const auto& preset=PresetManager::getFactoryPreset(i);
        const int category=juce::roundToInt(preset.values.at(Params::IDs::CATEGORY));
        const int mode=Params::modeToMenuIndex(category,juce::roundToInt(preset.values.at(Params::IDs::MODE)));
        ++actualCounts[static_cast<std::size_t>(category)];
        grouped &= category>previousCategory || (category==previousCategory&&mode>=previousMode);
        if(category!=previousCategory)previousMode=-1;
        previousCategory=category;previousMode=mode;
    }
    check(actualCounts==expectedCounts&&grouped,"预设按Plate、Room、Hall、Chamber及各自模式连续分组");
    PresetManager::applyPreset(PresetManager::getFactoryPreset(8),processor.apvts);
    const auto expected=PresetManager::snapshotCurrent({}, {},processor.apvts).values;
    const auto file=PresetManager::saveUserPreset(juce::String::fromUTF8("钢琴复听"),processor.apvts,directory);
    const auto firstText=file.loadFileAsString();
    const auto duplicate=PresetManager::saveUserPreset(juce::String::fromUTF8("钢琴复听"),processor.apvts,directory);
    check(file.existsAsFile() && duplicate.existsAsFile() && file!=duplicate
        && file.loadFileAsString()==firstText,"中文名称可保存，重名另存不覆盖");
    check(PresetManager::getUserPresetFiles(directory).size()==2,"用户预设可被重新枚举");
    PresetManager::applyPreset(PresetManager::getFactoryPreset(0),processor.apvts);
    check(PresetManager::loadUserPreset(file,processor.apvts)
        && PresetManager::snapshotCurrent({}, {},processor.apvts).values==expected,"保存后能准确恢复真实插件全部参数");
    const auto bad=directory.getChildFile("bad.sanmureverb");
    bad.replaceWithText("<Parameters><PARAM id=\"decay\" value=\"nan\"/></Parameters>");
    check(!PresetManager::loadUserPreset(bad,processor.apvts)
        && PresetManager::snapshotCurrent({}, {},processor.apvts).values==expected,"损坏预设被拒绝且不改变当前参数");
    auto state=processor.apvts.copyState();state.getChildWithProperty("id","decay").setProperty("value","oops",nullptr);
    state.createXml()->writeTo(bad);
    check(!PresetManager::loadUserPreset(bad,processor.apvts),"完整但包含非法参数文本的文件被拒绝");
    state.getChildWithProperty("id","decay").setProperty("value",1000,nullptr);state.createXml()->writeTo(bad);
    check(!PresetManager::loadUserPreset(bad,processor.apvts),"超出范围的预设被拒绝");
    check(PresetManager::saveUserPreset("  ",processor.apvts,directory)==juce::File(),"空名称不会写文件");
    const auto safe=PresetManager::saveUserPreset("../../test",processor.apvts,directory);
    check(safe.existsAsFile() && safe.getParentDirectory()==directory,"名称不能越过预设目录");
    // 模式拆分后，旧P3工程与用户预设不能被误解释成新钢板。
    for(int category=0;category<4;++category)for(int mode=0;mode<3;++mode)
    {
        auto legacy=processor.apvts.copyState();legacy.removeProperty(state_compatibility::plateStyleVersion,nullptr);
        legacy.getChildWithProperty("id","category").setProperty("value",category,nullptr);
        legacy.getChildWithProperty("id","mode").setProperty("value",mode,nullptr);
        auto xml=legacy.createXml();juce::MemoryBlock bytes;juce::AudioProcessor::copyXmlToBinary(*xml,bytes);
        processor.setStateInformation(bytes.getData(),static_cast<int>(bytes.getSize()));
        const int expectedMode=category==0?0:mode;
        check(processor.apvts.getRawParameterValue("mode")->load()==expectedMode,
              "旧宿主状态恢复保持P3声音及其他类型模式");
        xml->writeTo(bad);
        check(PresetManager::loadUserPreset(bad,processor.apvts)
              &&processor.apvts.getRawParameterValue("mode")->load()==expectedMode,
              "旧用户预设按同一规则迁移");
    }
    PresetManager::applyPreset(PresetManager::getFactoryPreset(3),processor.apvts);
    juce::MemoryBlock current;processor.getStateInformation(current);
    PresetManager::applyPreset(PresetManager::getFactoryPreset(0),processor.apvts);
    processor.setStateInformation(current.getData(),static_cast<int>(current.getSize()));
    check(processor.apvts.getRawParameterValue("category")->load()==0
          &&processor.apvts.getRawParameterValue("mode")->load()==1,"新钢板宿主状态往返保留钢板模式");
    const auto steelFile=PresetManager::saveUserPreset(juce::String::fromUTF8("钢板复听"),processor.apvts,directory);
    PresetManager::applyPreset(PresetManager::getFactoryPreset(0),processor.apvts);
    check(PresetManager::loadUserPreset(steelFile,processor.apvts)
          &&processor.apvts.getRawParameterValue("mode")->load()==1,"新钢板用户预设往返保留钢板模式");
    check(Params::getModeNamesForCategory(0)==juce::StringArray{"Modern Silk","Classic Steel"},
          "板式菜单只有两个已实现风格，丝滑排首位");
    PresetManager::applyPreset(PresetManager::getFactoryPreset(15),processor.apvts);
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        auto* panel=dynamic_cast<SanMuReverbAudioProcessorEditor*>(editor.get());
        check(panel!=nullptr,"真实编辑器提供新界面的宿主参数连接");
        if(panel)
        {
            auto data=panel->handleCommand("initialize",{});data.getDynamicObject()->setProperty("mode",2);
            panel->handleCommand("apply",data);
            check(processor.apvts.getRawParameterValue("category")->load()==0
                  &&processor.apvts.getRawParameterValue("mode")->load()==0,"从其他类型进入板式默认选择Silk");
            PresetManager::applyPreset(PresetManager::getFactoryPreset(3),processor.apvts);
            data=panel->handleCommand("initialize",{});panel->handleCommand("apply",data);
            check(processor.apvts.getRawParameterValue("mode")->load()==1,"重复点击当前板式类别保留已选钢板");
        }
    }
    check(Params::getModeNamesForCategory(1)==juce::StringArray{"Studio Room","Live Room"}
          &&Params::getModeNamesForCategory(2)==juce::StringArray{"Concert Hall","Cathedral"},"房间和大厅菜单各保留两种风格");
    for(int category=0;category<4;++category)
        check(Params::getModeNamesForCategory(category).size()==2,"四种类型均只有两个可见模式");
    check(Params::getModeNamesForCategory(3)==juce::StringArray{"Velvet Chamber","Stone Chamber"}
          &&Params::modeToMenuIndex(3,2)==0,"回声室新名称和第三编号兼容正确");
    for(int category=0;category<4;++category)
        for(int index=0;index<Params::getModeNamesForCategory(category).size();++index)
            check(Params::modeToMenuIndex(category,Params::menuIndexToMode(category,index))==index,"可见菜单与宿主编号往返一致");
    check(Params::menuIndexToMode(2,1)==2&&Params::modeToMenuIndex(2,1)==0
          &&Params::modeToMenuIndex(1,2)==1,"Cathedral编号不变，旧编号映射明确");
    for(int index:{4,6,17,20})
    {
        PresetManager::applyPreset(PresetManager::getFactoryPreset(index),processor.apvts);
        juce::MemoryBlock memory;processor.getStateInformation(memory);
        PresetManager::applyPreset(PresetManager::getFactoryPreset(0),processor.apvts);
        processor.setStateInformation(memory.getData(),static_cast<int>(memory.getSize()));
        check(processor.apvts.getRawParameterValue("category")->load()==(index<17?1:3)
              &&processor.apvts.getRawParameterValue("mode")->load()==(index==4||index==17?0:1),"房间与回声室宿主状态往返正确");
    }
    if(argc==2)
    {
        const auto output=juce::File::getCurrentWorkingDirectory().getChildFile(juce::String(argv[1]));
        output.createDirectory();
        output.getChildFile("factory_presets.json").replaceWithText(juce::JSON::toString(exports));
        for(int index:{8,15,0,3,4,6,17,20})
        {
            PresetManager::applyPreset(PresetManager::getFactoryPreset(index),processor.apvts);
            std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
            const auto picture=editor->createComponentSnapshot(editor->getLocalBounds());
            const auto path=output.getChildFile(index==8?"concert.png":index==15?"cathedral.png":index==0?"plate.png":index==3?"steel.png":index==4?"studio.png":index==6?"live.png":index==17?"velvet.png":"stone.png");
            juce::FileOutputStream stream(path);juce::PNGImageFormat format;
            check(format.writeImageToStream(picture,stream),"真实编辑器预览可导出");
        }
    }
    directory.deleteRecursively();
    std::printf("预设检查：%d项失败\n",failures);
    return failures?1:0;
}
