#include "bridge.hpp"
#define REAPERAPI_IMPLEMENT
#include "adapter.hpp"
#include <cstring>
#include <iostream>

int main() {
    using namespace rmcp;
    EnumProjects = [](int i,char* out,int size) -> ReaProject* {
        if(out && size>0) out[0]=0;
        return i==0 || i==-1 ? reinterpret_cast<ReaProject*>(1) : nullptr;
    };
    GetProjectName = [](ReaProject*,char* out,int size) { if(size>4) std::strcpy(out,"Test"); };
    GetProjectStateChangeCount = [](ReaProject*) { return 7; };
    CountTracks = [](ReaProject*) { return 0; };
    IsProjectDirty = [](ReaProject*) { return 1; };
    GetAppVersion = []() -> const char* { return "7.80"; };
    add_project_operations();
    try {
        auto p=dispatch("project.get",json::object(),"read-only");
        if(p.at("name")!="Test") throw std::runtime_error("project mismatch");
        json args={{"project_id",p.at("project_id")},{"confirm",true}};
        try { dispatch("project.save",args,"read-only"); throw std::runtime_error("policy bypass"); }
        catch(const Error& e) { if(e.code!="PERMISSION_DENIED") throw; }
        args["confirm"]=false;
        try { dispatch("project.save",args,"confirm-destructive"); throw std::runtime_error("confirmation bypass"); }
        catch(const Error& e) { if(e.code!="CONFIRMATION_REQUIRED") throw; }
        args["confirm"]=true;
        try { dispatch("project.save",args,"confirm-destructive"); throw std::runtime_error("unsaved accepted"); }
        catch(const Error& e) { if(e.code!="UNSAVED_PROJECT") throw; }
        args["project_id"]="old-project";
        try { dispatch("project.save",args,"confirm-destructive"); throw std::runtime_error("stale project accepted"); }
        catch(const Error& e) { if(e.code!="PROJECT_CHANGED") throw; }
        std::cout<<"Native project checks passed\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
