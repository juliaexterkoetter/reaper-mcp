#pragma once
#include "adapter.hpp"
namespace rmcp {
inline std::string track_guid(MediaTrack* t) {
    char buf[128]{};
    if(!GetSetMediaTrackInfo_String(t,"GUID",buf,false)) throw Error("INVALID_TRACK", "Cannot read track GUID");
    return buf;
}
inline std::string track_name(MediaTrack* t) {
    char buf[4096]{}; GetTrackName(t,buf,sizeof(buf)); return buf;
}
inline MediaTrack* track(ReaProject* proj,const std::string& selector) {
    MediaTrack* found=nullptr;
    for(int i=0;i<CountTracks(proj);++i) {
        auto* t=GetTrack(proj,i);
        if(!t) continue;
        if(track_guid(t)==selector) return t;
        if(track_name(t)==selector) {
            if(found) throw Error("AMBIGUOUS_TRACK", "Use a GUID: more than one track has this name");
            found=t;
        }
    }
    if(!found) throw Error("TRACK_NOT_FOUND", "Track not found; inspect the project again");
    return found;
}
inline json gain_db(double gain) { return gain>0 ? json(20*std::log10(gain)) : json(nullptr); }
inline double volume(const json& p,double current) {
    if(p.at("volume_db").is_null()) {
        if(p.value("relative",false)) throw Error("INVALID_PARAMETER", "Relative silence is undefined");
        return 0;
    }
    double db=number(p,"volume_db",-150,24);
    if(p.value("relative",false)) {
        if(current<=0) throw Error("INVALID_PARAMETER", "Set an absolute volume to leave silence");
        db+=20*std::log10(current);
    }
    if(db < -150 || db > 24) throw Error("INVALID_PARAMETER", "Result outside -150 to +24 dB");
    return std::pow(10.0,db/20);
}
inline json track_info(MediaTrack* t) {
    return {{"guid",track_guid(t)},{"name",track_name(t)},
        {"volume_db",gain_db(GetMediaTrackInfo_Value(t,"D_VOL"))},
        {"pan_percent",100*GetMediaTrackInfo_Value(t,"D_PAN")},
        {"muted",GetMediaTrackInfo_Value(t,"B_MUTE")!=0},
        {"solo",GetMediaTrackInfo_Value(t,"I_SOLO")!=0}};
}
inline void set_track(MediaTrack* t,const char* key,double value) {
    if(!SetMediaTrackInfo_Value(t,key,value)) throw Error("EDIT_FAILED", "Track property was not changed");
}
inline void add_track_operations() {
    operations["tracks.list"]={"read",false,[](const json& p){
        auto* proj=project(p); json result=json::array();
        if(CountTracks(proj)>2000) throw Error("RESULT_LIMIT", "Project exceeds 2000-track safety limit");
        for(int i=0;i<CountTracks(proj);++i) result.push_back(track_info(GetTrack(proj,i)));
        return result;
    }};
    operations["tracks.get"]={"read",false,[](const json& p){return track_info(track(project(p),text(p,"track",1024)));}};
    operations["tracks.volume"]={"write",true,[](const json& p){
        auto* t=track(project(p),text(p,"track",1024));
        set_track(t,"D_VOL",volume(p,GetMediaTrackInfo_Value(t,"D_VOL"))); return track_info(t);
    }};
    operations["tracks.pan"]={"write",true,[](const json& p){
        auto* t=track(project(p),text(p,"track",1024)); set_track(t,"D_PAN",number(p,"pan_percent",-100,100)/100);
        return track_info(t);
    }};
    operations["tracks.rename"]={"write",true,[](const json& p){
        auto* t=track(project(p),text(p,"track",1024)); auto name=text(p,"name",1024);
        if(!GetSetMediaTrackInfo_String(t,"P_NAME",name.data(),true)) throw Error("EDIT_FAILED", "Rename failed");
        return track_info(t);
    }};
    operations["tracks.create"]={"write",true,[](const json& p){
        auto* proj=project(p); auto name=text(p,"name",1024); int index=CountTracks(proj);
        InsertTrackAtIndex(index,true); auto* t=GetTrack(proj,index);
        if(!t) throw Error("EDIT_FAILED", "Track creation failed");
        if(!GetSetMediaTrackInfo_String(t,"P_NAME",name.data(),true)) throw Error("EDIT_FAILED", "Track created but naming failed; use Undo");
        TrackList_AdjustWindows(false); return track_info(t);
    }};
    operations["tracks.delete"]={"destructive",true,[](const json& p){
        auto* t=track(project(p),text(p,"track",1024)); DeleteTrack(t); TrackList_AdjustWindows(false);
        return json{{"deleted",true}};
    }};
    for(auto action:{"mute","unmute","solo","unsolo"}) {
        std::string a=action;
        operations["tracks."+a]={"write",true,[a](const json& p){
            auto* t=track(project(p),text(p,"track",1024));
            set_track(t,(a=="mute" || a=="unmute")?"B_MUTE":"I_SOLO",(a=="mute" || a=="solo")?1:0);
            return track_info(t);
        }};
    }
}
}
