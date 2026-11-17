#pragma once
#include "fx.hpp"
namespace rmcp {
inline json resource_files(const std::filesystem::path& root,const std::string& extension) {
    json files=json::array();std::error_code ec;bool truncated=false;int visited=0;
    if(platform::inspect(root).symlink)
        return {{"files",files},{"truncated",false},{"read_error",true}};
    auto it=std::filesystem::recursive_directory_iterator(root,std::filesystem::directory_options::skip_permission_denied,ec);
    const auto end=std::filesystem::recursive_directory_iterator();
    for(;it!=end && !ec;it.increment(ec)){
        if(++visited>10000 || files.size()>=500){truncated=true;break;}
        auto kind=platform::inspect(it->path());
        if(!kind.readable || kind.symlink) {it.disable_recursion_pending();continue;}
        if(it->is_regular_file(ec) && lowercase(it->path().extension().u8string())==extension)
            files.push_back(it->path().lexically_relative(root).u8string());
    }
    return {{"files",files},{"truncated",truncated},{"read_error",ec && ec!=std::errc::no_such_file_or_directory}};
}
inline void add_environment_operations(){
    operations["environment.get"]={"read",false,[](const json&){
        refresh_projects();auto* proj=EnumProjects(-1,nullptr,0);char rate[128]{};
        json result={{"reaper_version",GetAppVersion()},{"extension_version","0.1.0a1"},
            {"protocol_version",1},{"resource_directory",GetResourcePath()},
            {"available_fx_tool","reaper_list_available_fx"},{"render_preset_application","planned"}};
        result["project"]=proj?project_info(proj,-1):json(nullptr);
        result["device_sample_rate"]=GetAudioDeviceInfo("SRATE",rate,sizeof(rate))?json(rate):json(nullptr);
        result["project_sample_rate"]=proj && GetSetProjectInfo(proj,"PROJECT_SRATE_USE",0,false)!=0
            ?json(GetSetProjectInfo(proj,"PROJECT_SRATE",0,false)):json(nullptr);
        return result;
    }};
    operations["environment.presets"]={"read",false,[](const json&){
        auto root=std::filesystem::u8path(GetResourcePath());
        return json{{"fx_chains",resource_files(root/"FXChains",".rfxchain")},
            {"track_templates",resource_files(root/"TrackTemplates",".rtracktemplate")},
            {"render_presets",{{"status","planned"},{"reason","No stable enumeration/application API implemented"}}}};
    }};
}
}
