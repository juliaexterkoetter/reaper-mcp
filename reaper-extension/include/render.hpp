#pragma once
#include "fx.hpp"
namespace rmcp {
inline std::string project_string(ReaProject* proj,const char* key) {
    std::vector<char> buf(1024*1024,0);
    if(!GetSetProjectInfo_String(proj,key,buf.data(),false))throw Error("READ_FAILED","Cannot read project setting");
    return buf.data();
}
inline void set_project_string(ReaProject* proj,const char* key,std::string value) {
    if(!GetSetProjectInfo_String(proj,key,value.data(),true))throw Error("EDIT_FAILED","Cannot set project setting");
}
inline json render_settings(ReaProject* proj) {
    return {{"sample_rate",GetSetProjectInfo(proj,"RENDER_SRATE",0,false)},
        {"channels",GetSetProjectInfo(proj,"RENDER_CHANNELS",0,false)},
        {"bounds",GetSetProjectInfo(proj,"RENDER_BOUNDSFLAG",0,false)},
        {"start_seconds",GetSetProjectInfo(proj,"RENDER_STARTPOS",0,false)},
        {"end_seconds",GetSetProjectInfo(proj,"RENDER_ENDPOS",0,false)},
        {"settings_flags",GetSetProjectInfo(proj,"RENDER_SETTINGS",0,false)},
        {"output_directory",project_string(proj,"RENDER_FILE")},
        {"pattern",project_string(proj,"RENDER_PATTERN")}};
}
inline json render_job={{"state","idle"}};
inline json pending_render;
inline void validate_render(ReaProject* proj) {
    int settings=static_cast<int>(GetSetProjectInfo(proj,"RENDER_SETTINGS",0,false));
    int bounds=static_cast<int>(GetSetProjectInfo(proj,"RENDER_BOUNDSFLAG",0,false));
    if((settings&(1|2|8|32|64|128|4096|8192|(8<<16))) || bounds<0 || bounds>2 ||
        GetSetProjectInfo(proj,"RENDER_ADDTOPROJ",0,false)!=0 || !project_string(proj,"RENDER_FORMAT2").empty())
        throw Error("UNSUPPORTED_RENDER_MODE","Use single master-mix render, no secondary format or add-to-project");
    const char* label=kbd_getTextFromCmd(41824,nullptr);
    auto name=lowercase(label?label:"");
    if(name.find("render project")==std::string::npos || name.find("most recent render settings")==std::string::npos)
        throw Error("UNSUPPORTED_RENDER_ACTION","This REAPER build/language does not expose the verified render action");
}
struct RestoreRender {
    ReaProject* proj;std::string file,pattern,extra;
    explicit RestoreRender(ReaProject* p):proj(p),file(project_string(p,"RENDER_FILE")),
        pattern(project_string(p,"RENDER_PATTERN")),extra(project_string(p,"RENDER_EXTRAFILEDIR")){}
    ~RestoreRender(){
        GetSetProjectInfo_String(proj,"RENDER_FILE",file.data(),true);
        GetSetProjectInfo_String(proj,"RENDER_PATTERN",pattern.data(),true);
        GetSetProjectInfo_String(proj,"RENDER_EXTRAFILEDIR",extra.data(),true);
    }
};
inline void run_pending_render() {
    if(pending_render.is_null())return;
    auto args=pending_render;pending_render=nullptr;
    try {
        auto* proj=project(args);validate_render(proj);RestoreRender restore(proj);
        auto root=std::filesystem::u8path(render_job.at("output_directory").get<std::string>());
        set_project_string(proj,"RENDER_FILE",root.u8string());
        set_project_string(proj,"RENDER_PATTERN","render");
        set_project_string(proj,"RENDER_EXTRAFILEDIR",root.u8string());
        auto targets=project_string(proj,"RENDER_TARGETS");
        // Exactly one primary output, entirely inside the newly-created directory.
        if(targets.empty() || targets.find(';')!=std::string::npos)throw Error("UNSAFE_RENDER_TARGET","Expected one render target");
        auto target=std::filesystem::u8path(targets).lexically_normal();
        if(target.parent_path()!=root || std::filesystem::exists(target))throw Error("UNSAFE_RENDER_TARGET","Render target is outside the reserved directory or already exists");
        render_job["state"]="running";render_job["target"]=target.u8string();
        Main_OnCommandEx(41824,0,proj);
        if(!std::filesystem::is_regular_file(target) || std::filesystem::file_size(target)==0)
            throw Error("RENDER_FAILED","No nonempty output was produced; render may have been cancelled");
        render_job["state"]="output-produced";
        render_job["bytes"]=std::filesystem::file_size(target);
        render_job["note"]="Output exists; audition it. Plugin/render quality is not automatically verified.";
    }catch(const Error& e){render_job["state"]="failed";render_job["error"]={{"code",e.code},{"message",e.what()}};}
    catch(...){render_job["state"]="failed";render_job["error"]={{"code","RENDER_FAILED"},{"message","Render failed; inspect REAPER and output directory"}};}
}
inline void add_render_operations() {
    operations["render.settings"]={"read",false,[](const json& p){return render_settings(project(p));}};
    operations["render.set_settings"]={"write",true,[](const json& p){auto* proj=project(p);
        int rate=integer(p,"sample_rate",8000,384000),channels=integer(p,"channels",1,64);
        GetSetProjectInfo(proj,"RENDER_SRATE",rate,true);GetSetProjectInfo(proj,"RENDER_CHANNELS",channels,true);return render_settings(proj);}};
    operations["render.status"]={"read",false,[](const json& p){project(p);return render_job;}};
    operations["render.start"]={"render",false,[](const json& p){auto* proj=project(p);validate_render(proj);
        if(!pending_render.is_null())throw Error("RENDER_BUSY","A render is already queued");
        char path[32768]{};EnumProjects(-1,path,sizeof(path));
        if(!path[0])throw Error("UNSAVED_PROJECT","Save the project once before rendering");
        std::filesystem::path parent;
        if(p.contains("output_directory") && !p.at("output_directory").is_null()) parent=std::filesystem::u8path(text(p,"output_directory",32700));
        else {auto configured=project_string(proj,"RENDER_FILE");parent=configured.empty()?std::filesystem::path("renders"):std::filesystem::u8path(configured);
            if(parent.is_relative())parent=std::filesystem::u8path(path).parent_path()/parent;}
        if(!parent.is_absolute() || platform::is_network_path(parent))
            throw Error("INVALID_PATH","Render output must be an absolute local filesystem directory");
        std::filesystem::create_directories(parent);parent=std::filesystem::canonical(parent);
        if(platform::is_network_path(parent))
            throw Error("INVALID_PATH","Resolved render output must remain on a local filesystem");
        auto name="reaper-mcp-"+std::to_string(platform::process_id())+"-"+std::to_string(platform::tick_count());auto root=parent/name;
        if(!std::filesystem::create_directory(root))throw Error("OUTPUT_EXISTS","Output reservation failed; inspect before retrying");
        render_job={{"state","queued"},{"job_id",name},{"output_directory",root.u8string()}};pending_render=p;return render_job;
    }};
}
}
