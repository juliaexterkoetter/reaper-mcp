#include "bridge.hpp"
#define REAPERAPI_IMPLEMENT
#include "render.hpp"
#include "environment.hpp"
#include <cstring>
#include <iostream>

std::map<std::string,std::string> strings;
std::filesystem::path base;
std::string target_mode="plain",fail_set;
bool create_output=true,fail_action=false;
int render_calls=0,sequence=0;
double sample_rate=0,original_rate=0;

void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void restored(){
    require(sample_rate==original_rate,"sample rate not restored");
    require(strings["RENDER_FILE"]=="original","directory not restored");
    require(strings["RENDER_PATTERN"]=="original-pattern","pattern not restored");
    require(strings["RENDER_EXTRAFILEDIR"]=="original-extra","extra directory not restored");
}
int main(){
    using namespace rmcp;
    base=std::filesystem::temp_directory_path()/("rmcp-render-test-"+std::to_string(GetCurrentProcessId()));
    std::filesystem::create_directories(base);
    EnumProjects=[](int i,char* out,int size)->ReaProject*{
        auto path=(base/"test.rpp").u8string();
        if(out && size>static_cast<int>(path.size()))std::strcpy(out,path.c_str());
        return i==0||i==-1?reinterpret_cast<ReaProject*>(1):nullptr;};
    GetSetProjectInfo=[](ReaProject*,const char* key,double value,bool set){
        if(std::string(key)!="RENDER_SRATE")return 0.0;
        if(set)sample_rate=value;return sample_rate;};
    GetSetProjectInfo_String=[](ReaProject*,const char* key,char* value,bool set){
        if(set){
            if(fail_set==key){fail_set.clear();return false;}
            strings[key]=value;
        }else if(std::string(key)=="RENDER_TARGETS"){
            if(target_mode=="read-failure")return false;
            auto root=std::filesystem::u8path(strings["RENDER_FILE"]);
            auto path=(root/"render.wav").u8string();
            std::string result=path;
            if(target_mode=="terminated")result+=";"; // Observed in REAPER 7.84.
            if(target_mode=="empty")result="";
            if(target_mode=="delimiter-only")result=";";
            if(target_mode=="leading")result=";"+path;
            if(target_mode=="extra-empty")result=path+";;";
            if(target_mode=="multiple")result=path+";"+(root/"second.wav").u8string();
            if(target_mode=="multiple-terminated")result=path+";"+path+";";
            if(target_mode=="outside")result=(base/"outside.wav").u8string()+";";
            if(target_mode=="traversal")result=(root/".."/"outside.wav").u8string()+";";
            if(target_mode=="existing")std::ofstream(root/"render.wav")<<"do not overwrite";
            std::strcpy(value,result.c_str());
        }else std::strcpy(value,strings[key].c_str());return true;};
    kbd_getTextFromCmd=[](int,KbdSectionInfo*)->const char*{return "File: Render project, using the most recent render settings";};
    Main_OnCommandEx=[](int command,int,ReaProject*){
        require(command==41824,"wrong action");++render_calls;
        sample_rate=44100; // Real host resolves the automatic sample rate.
        if(fail_action)throw std::runtime_error("simulated action failure");
        if(create_output)std::ofstream(std::filesystem::u8path(strings["RENDER_FILE"])/"render.wav")<<"fake test output";};
    strings["RENDER_FILE"]="original";strings["RENDER_PATTERN"]="original-pattern";strings["RENDER_EXTRAFILEDIR"]="original-extra";
    refresh_projects();add_render_operations();
    try{
        auto run=[&](const std::string& mode,const std::string& expected,bool invokes){
            target_mode=mode;
            json args={{"project_id",project_ids.begin()->second},{"confirm",true},
                {"output_directory",(base/std::to_string(++sequence)).u8string()}};
            const int before=render_calls;
            auto job=dispatch("render.start",args,"confirm-destructive");
            require(job.at("state")=="queued" && render_calls==before,"render not deferred");
            run_pending_render();restored();
            require(render_calls==before+(invokes?1:0),"unexpected render action");
            if(expected.empty())require(render_job.at("state")=="output-produced","valid target rejected");
            else require(render_job.at("state")=="failed" && render_job.at("error").at("code")==expected,"wrong failure");
            if(mode=="existing"){
                std::ifstream file(std::filesystem::u8path(render_job.at("output_directory").get<std::string>())/"render.wav");
                std::string content;std::getline(file,content);require(content=="do not overwrite","existing file overwritten");
            }
        };
        run("plain","",true);
        run("terminated","",true);
        for(const auto* mode:{"empty","delimiter-only","leading","extra-empty","multiple","multiple-terminated","outside","traversal","existing"})
            run(mode,"UNSAFE_RENDER_TARGET",false);
        run("read-failure","READ_FAILED",false);
        for(const auto* key:{"RENDER_FILE","RENDER_PATTERN","RENDER_EXTRAFILEDIR"}){
            fail_set=key;run("terminated","EDIT_FAILED",false);
        }
        original_rate=sample_rate=48000;run("terminated","",true);
        create_output=false;run("terminated","RENDER_FAILED",true);
        create_output=true;fail_action=true;run("terminated","RENDER_FAILED",true);
        std::ofstream(base/"voice.RfxChain")<<"test fixture";
        auto presets=resource_files(base,".rfxchain");
        require(presets.at("files").size()==1 && presets.at("read_error")==false,"preset discovery failed");
        std::filesystem::remove_all(base);std::cout<<"Render isolation checks passed ("<<sequence<<" cases)\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';std::filesystem::remove_all(base);return 1;}
}
