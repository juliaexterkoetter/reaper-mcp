#include "bridge.hpp"
#define REAPERAPI_IMPLEMENT
#include "adapter.hpp"
#include "tracks.hpp"
#include "transport.hpp"
#include "fx.hpp"
#include "markers.hpp"
#include "automation.hpp"
#include <cstring>
#include <iostream>

double test_gain=1.0;
int undo_begin=0,undo_end=0;
int playback=0;
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
        CountTracks=[](ReaProject*){return 2;};
        GetTrack=[](ReaProject*,int index)->MediaTrack*{return reinterpret_cast<MediaTrack*>(static_cast<uintptr_t>(index+1));};
        GetTrackName=[](MediaTrack*,char* out,int){std::strcpy(out,"Voz");return true;};
        GetSetMediaTrackInfo_String=[](MediaTrack* t,const char*,char* out,bool){
            std::strcpy(out,t==reinterpret_cast<MediaTrack*>(1)?"{A}":"{B}");return true;};
        GetMediaTrackInfo_Value=[](MediaTrack*,const char* key){return std::string(key)=="D_VOL"?test_gain:0.0;};
        SetMediaTrackInfo_Value=[](MediaTrack*,const char*,double v){test_gain=v;return true;};
        Undo_BeginBlock2=[](ReaProject*){++undo_begin;};
        Undo_EndBlock2=[](ReaProject*,const char*,int){++undo_end;};
        UpdateArrange=[](){};
        add_track_operations();
        args["project_id"]=p.at("project_id"); args["track"]="Voz";
        try { dispatch("tracks.get",args,"read-only"); throw std::runtime_error("ambiguous name accepted"); }
        catch(const Error& e) { if(e.code!="AMBIGUOUS_TRACK") throw; }
        args["track"]="{A}"; args["volume_db"]=-6.0; args["relative"]=true;
        auto t=dispatch("tracks.volume",args,"confirm-destructive");
        if(std::abs(t.at("volume_db").get<double>()+6)>1e-9 || undo_begin!=1 || undo_end!=1)
            throw std::runtime_error("volume/undo mismatch");
        args["volume_db"]=nullptr; args["relative"]=false;
        t=dispatch("tracks.volume",args,"confirm-destructive");
        if(!t.at("volume_db").is_null()) throw std::runtime_error("silence mismatch");
        GetPlayStateEx=[](ReaProject*){return playback;};
        OnPlayButtonEx=[](ReaProject*){playback=1;};OnStopButtonEx=[](ReaProject*){playback=0;};
        OnPauseButtonEx=[](ReaProject*){playback^=2;};add_transport_operations();
        dispatch("transport.play",args,"confirm-destructive");
        dispatch("transport.pause",args,"confirm-destructive");
        dispatch("transport.pause",args,"confirm-destructive");
        if(!(playback&2))throw std::runtime_error("pause toggled playback");
        dispatch("transport.stop",args,"confirm-destructive");
        if(playback)throw std::runtime_error("stop failed");
        EnumInstalledFX=[](int i,const char** n,const char** id){if(i>0)return false;*n="VST: Test";*id="test.dll";return true;};
        add_fx_operations();
        auto available=dispatch("fx.available",json{{"query","TEST"}},"read-only");
        if(available.at("total")!=1)throw std::runtime_error("plugin discovery failed");
        args["plugin"]="not installed";
        try{dispatch("fx.add",args,"confirm-destructive");throw std::runtime_error("unknown plugin loaded");}
        catch(const Error& e){if(e.code!="PLUGIN_NOT_FOUND")throw;}
        GetNumRegionsOrMarkers=[](ReaProject*){return 0;};add_marker_operations();
        if(!dispatch("markers.list",args,"read-only").empty())throw std::runtime_error("empty markers failed");
        args["name"]="Region";args["position_seconds"]=5;args["end_seconds"]=4;
        try{dispatch("regions.create",args,"confirm-destructive");throw std::runtime_error("inverted region accepted");}
        catch(const Error& e){if(e.code!="INVALID_PARAMETER")throw;}
        add_automation_operations();args["expected_state_version"]=6;
        try{dispatch("automation.delete",args,"confirm-destructive");throw std::runtime_error("stale point accepted");}
        catch(const Error& e){if(e.code!="PROJECT_CHANGED")throw;}
        std::cout<<"Native project and track checks passed\n"; return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
