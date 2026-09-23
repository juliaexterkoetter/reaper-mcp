#include "bridge.hpp"
#define REAPERAPI_IMPLEMENT
#include "render.hpp"
#include "environment.hpp"
#include <cstring>
#include <iostream>
std::map<std::string,std::string> strings;
std::filesystem::path base;
bool escape_target=false,create_output=true;
int render_calls=0;
int main(){
    using namespace rmcp;
    base=std::filesystem::temp_directory_path()/("rmcp-render-test-"+std::to_string(rmcp::platform::process_id()));
    std::filesystem::create_directories(base);
    EnumProjects=[](int i,char* out,int size)->ReaProject*{
        auto path=(base/"test.rpp").u8string();
        if(out && size>static_cast<int>(path.size()))std::strcpy(out,path.c_str());
        return i==0||i==-1?reinterpret_cast<ReaProject*>(1):nullptr;};
    GetSetProjectInfo=[](ReaProject*,const char*,double,bool){return 0.0;};
    GetSetProjectInfo_String=[](ReaProject*,const char* key,char* value,bool set){
        if(set)strings[key]=value;
        else if(std::string(key)=="RENDER_TARGETS"){
            auto path=(escape_target?base:std::filesystem::u8path(strings["RENDER_FILE"]))/"render.wav";
            std::strcpy(value,path.u8string().c_str());
        }else std::strcpy(value,strings[key].c_str());return true;};
    kbd_getTextFromCmd=[](int,KbdSectionInfo*)->const char*{return "File: Render project, using the most recent render settings";};
    Main_OnCommandEx=[](int command,int,ReaProject*){if(command!=41824)throw std::runtime_error("wrong action");++render_calls;
        if(create_output)std::ofstream(std::filesystem::u8path(strings["RENDER_FILE"])/"render.wav")<<"fake test output";};
    strings["RENDER_FILE"]="original";strings["RENDER_PATTERN"]="original-pattern";strings["RENDER_EXTRAFILEDIR"]="original-extra";
    refresh_projects();add_render_operations();
    json args={{"project_id",project_ids.begin()->second},{"confirm",true},{"output_directory",base.u8string()}};
    try{
        auto job=dispatch("render.start",args,"confirm-destructive");
        if(job.at("state")!="queued" || render_calls)throw std::runtime_error("render not deferred");
        run_pending_render();
        if(render_job.at("state")!="output-produced" || render_calls!=1 || strings["RENDER_FILE"]!="original" || strings["RENDER_PATTERN"]!="original-pattern")throw std::runtime_error("render/restore failed");
        // Supply a fresh subdirectory to avoid same-millisecond reservation collision.
        args["output_directory"]=(base/"second").u8string();escape_target=true;
        dispatch("render.start",args,"confirm-destructive");run_pending_render();
        if(render_job.at("state")!="failed" || render_calls!=1 || strings["RENDER_FILE"]!="original")throw std::runtime_error("unsafe target accepted");
        std::ofstream(base/"voice.RfxChain")<<"test fixture";
        auto presets=resource_files(base,".rfxchain");
        if(presets.at("files").size()!=1 || presets.at("read_error")!=false)throw std::runtime_error("preset discovery failed");
        std::filesystem::remove_all(base);std::cout<<"Render isolation checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';std::filesystem::remove_all(base);return 1;}
}
