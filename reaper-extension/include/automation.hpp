#pragma once
#include "tracks.hpp"
namespace rmcp {
inline std::string envelope_guid(TrackEnvelope* env) {
    char guid[128]{};if(!GetSetEnvelopeInfo_String(env,"GUID",guid,false))throw Error("INVALID_ENVELOPE","Cannot read envelope GUID");return guid;
}
inline TrackEnvelope* envelope(const json& p) {
    auto* t=track(project(p),text(p,"track",1024));auto id=text(p,"envelope",128);
    for(int i=0;i<CountTrackEnvelopes(t);++i){auto* e=GetTrackEnvelope(t,i);if(e && envelope_guid(e)==id)return e;}
    throw Error("ENVELOPE_NOT_FOUND","Envelope not found");
}
inline json envelope_info(TrackEnvelope* env) {
    char name[4096]{};GetEnvelopeName(env,name,sizeof(name));
    return {{"guid",envelope_guid(env)},{"name",name},{"point_count",CountEnvelopePoints(env)},
        {"scaling_mode",GetEnvelopeScalingMode(env)}};
}
inline double point_value(const json& p,TrackEnvelope* env) {
    auto* t=track(project(p),text(p,"track",1024));auto unit=text(p,"unit",32);double value=0;
    if(unit=="db" && (env==GetTrackEnvelopeByChunkName(t,"<VOLENV") || env==GetTrackEnvelopeByChunkName(t,"<VOLENV2"))) {
        if(!p.at("value").is_null())value=std::pow(10.0,number(p,"value",-150,24)/20);
    }else if(unit=="pan_percent" && (env==GetTrackEnvelopeByChunkName(t,"<PANENV") || env==GetTrackEnvelopeByChunkName(t,"<PANENV2"))) {
        value=number(p,"value",-100,100)/100;
    }else throw Error("UNSUPPORTED_ENVELOPE","Point writes support built-in volume (db) and pan (pan_percent) only");
    return ScaleToEnvelopeMode(GetEnvelopeScalingMode(env),value);
}
inline void check_state(const json& p) {
    if(integer(p,"expected_state_version",0,2147483647)!=GetProjectStateChangeCount(project(p)))
        throw Error("PROJECT_CHANGED","Envelope point indices may have changed; read again");
}
inline void add_automation_operations() {
    operations["automation.list"]={"read",false,[](const json& p){auto* t=track(project(p),text(p,"track",1024));json result=json::array();
        int count=CountTrackEnvelopes(t);if(count>1000)throw Error("RESULT_LIMIT","Track exceeds 1000 envelopes");
        for(int i=0;i<count;++i)result.push_back(envelope_info(GetTrackEnvelope(t,i)));return result;}};
    operations["automation.get"]={"read",false,[](const json& p){auto* e=envelope(p);auto result=envelope_info(e);json points=json::array();
        int count=CountEnvelopePoints(e),offset=p.contains("offset")?integer(p,"offset",0,10000000):0;
        int limit=p.contains("limit")?integer(p,"limit",1,1000):200;
        for(int i=offset;i<std::min(count,offset+limit);++i){double time=0,value=0,tension=0;int shape=0;bool selected=false;
            if(!GetEnvelopePoint(e,i,&time,&value,&shape,&tension,&selected))throw Error("READ_FAILED","Envelope point unavailable");
            points.push_back({{"index",i},{"time_seconds",time},{"native_value",value},{"unscaled_value",ScaleFromEnvelopeMode(GetEnvelopeScalingMode(e),value)},
                {"shape",shape},{"tension",tension},{"selected",selected}});}
        result["points"]=points;result["state_version"]=GetProjectStateChangeCount(project(p));return result;}};
    for(bool update:{false,true}) {
        operations[update?"automation.update":"automation.create"]={"write",true,[update](const json& p){
            auto* e=envelope(p);double time=number(p,"time_seconds",0,8640000),value=point_value(p,e);
            int shape=p.contains("shape")?integer(p,"shape",0,5):0;double tension=p.contains("tension")?number(p,"tension",-1,1):0;
            bool selected=false,noSort=false,ok=false;
            if(update){check_state(p);int index=integer(p,"point_index",0,CountEnvelopePoints(e)-1);
                ok=SetEnvelopePoint(e,index,&time,&value,&shape,&tension,nullptr,&noSort);
            }else ok=InsertEnvelopePoint(e,time,value,shape,tension,selected,&noSort);
            if(!ok || !Envelope_SortPoints(e))throw Error("EDIT_FAILED","Envelope edit failed; inspect or Undo");
            return json{{"changed",true},{"reread_required",true}};
        }};
    }
    operations["automation.delete"]={"destructive",true,[](const json& p){check_state(p);auto* e=envelope(p);
        int index=integer(p,"point_index",0,CountEnvelopePoints(e)-1);
        if(!DeleteEnvelopePointEx(e,-1,index) || !Envelope_SortPoints(e))throw Error("EDIT_FAILED","Envelope point deletion failed");
        return json{{"deleted",true},{"reread_required",true}};}};
}
}
