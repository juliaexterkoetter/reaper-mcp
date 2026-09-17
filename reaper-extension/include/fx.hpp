#pragma once
#include "tracks.hpp"
#include <cctype>
namespace rmcp {
inline std::string guid_string(const GUID* guid) {
    if(!guid) throw Error("INVALID_GUID","Object has no GUID");char out[64]{};guidToString(guid,out);return out;
}
inline int fx_index(MediaTrack* t,const std::string& guid) {
    for(int i=0;i<TrackFX_GetCount(t);++i) if(guid_string(TrackFX_GetFXGUID(t,i))==guid) return i;
    throw Error("FX_NOT_FOUND","FX no longer exists on this track");
}
inline json fx_info(MediaTrack* t,int index) {
    char name[4096]{};TrackFX_GetFXName(t,index,name,sizeof(name));
    return {{"guid",guid_string(TrackFX_GetFXGUID(t,index))},{"name",name},{"enabled",TrackFX_GetEnabled(t,index)},
        {"parameter_count",TrackFX_GetNumParams(t,index)}};
}
inline json parameter_info(MediaTrack* t,int fx,int param) {
    char name[4096]{},formatted[4096]{};
    TrackFX_GetParamName(t,fx,param,name,sizeof(name));TrackFX_GetFormattedParamValue(t,fx,param,formatted,sizeof(formatted));
    return {{"index",param},{"name",name},{"normalized_value",TrackFX_GetParamNormalized(t,fx,param)},{"formatted_value",formatted}};
}
inline std::string lowercase(std::string value) {
    for(auto& c:value) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return value;
}
inline void add_fx_operations() {
    operations["fx.available"]={"read",false,[](const json& p){
        auto query=lowercase(p.value("query",std::string()));
        if(query.size()>256)throw Error("INVALID_PARAMETER","Query too long");
        int offset=p.contains("offset")?integer(p,"offset",0,100000):0;
        int limit=p.contains("limit")?integer(p,"limit",1,500):100;int matched=0;json result=json::array();
        const char* name=nullptr;const char* ident=nullptr;
        for(int i=0;i<100000 && EnumInstalledFX(i,&name,&ident);++i) {
            if(!name || !ident || lowercase(name).find(query)==std::string::npos)continue;
            if(matched>=offset && static_cast<int>(result.size())<limit)result.push_back({{"name",name},{"identifier",ident}});
            ++matched;
        }
        return json{{"plugins",result},{"total",matched},{"offset",offset}};
    }};
    operations["fx.list"]={"read",false,[](const json& p){auto* t=track(project(p),text(p,"track",1024));
        json result=json::array();int count=TrackFX_GetCount(t);
        if(count>1000)throw Error("RESULT_LIMIT","Track exceeds 1000 FX");
        for(int i=0;i<count;++i)result.push_back(fx_info(t,i));return result;
    }};
    operations["fx.get"]={"read",false,[](const json& p){auto* t=track(project(p),text(p,"track",1024));return fx_info(t,fx_index(t,text(p,"fx",128)));}};
    operations["fx.add"]={"write",true,[](const json& p){
        auto* t=track(project(p),text(p,"track",1024));auto plugin=text(p,"plugin");
        std::string match;int matches=0;const char* name=nullptr;const char* ident=nullptr;
        for(int i=0;i<100000 && EnumInstalledFX(i,&name,&ident);++i) {
            if(ident && plugin==ident){match=ident;matches=1;break;}
            if(name && ident && plugin==name){match=ident;++matches;}
        }
        if(!matches)throw Error("PLUGIN_NOT_FOUND","Discover installed plugins first");
        if(matches>1)throw Error("AMBIGUOUS_PLUGIN","Use the exact plugin identifier");
        int index=TrackFX_AddByName(t,match.c_str(),false,-1);
        if(index<0)throw Error("PLUGIN_LOAD_FAILED","REAPER could not load this plugin");return fx_info(t,index);
    }};
    operations["fx.remove"]={"destructive",true,[](const json& p){auto* t=track(project(p),text(p,"track",1024));
        if(!TrackFX_Delete(t,fx_index(t,text(p,"fx",128))))throw Error("EDIT_FAILED","FX deletion failed");return json{{"deleted",true}};}};
    for(auto action:{"enable","disable","bypass"}){std::string a=action;
        operations["fx."+a]={"write",true,[a](const json& p){auto* t=track(project(p),text(p,"track",1024));int index=fx_index(t,text(p,"fx",128));
            TrackFX_SetEnabled(t,index,a=="enable");return fx_info(t,index);}};
    }
    operations["fx.parameters"]={"read",false,[](const json& p){auto* t=track(project(p),text(p,"track",1024));int index=fx_index(t,text(p,"fx",128));
        int count=TrackFX_GetNumParams(t,index),offset=p.contains("offset")?integer(p,"offset",0,100000):0;
        int limit=p.contains("limit")?integer(p,"limit",1,500):100;json result=json::array();
        for(int i=offset;i<std::min(count,offset+limit);++i)result.push_back(parameter_info(t,index,i));
        return json{{"parameters",result},{"total",count},{"offset",offset}};
    }};
    for(auto method:{"parameter","set_parameter"}){bool write=std::string(method)=="set_parameter";
        operations["fx."+std::string(method)]={write?"write":"read",write,[write](const json& p){
            auto* t=track(project(p),text(p,"track",1024));int index=fx_index(t,text(p,"fx",128));
            int param=integer(p,"parameter",0,TrackFX_GetNumParams(t,index)-1);
            if(write && !TrackFX_SetParamNormalized(t,index,param,number(p,"normalized_value",0,1)))throw Error("EDIT_FAILED","Parameter write failed");
            return parameter_info(t,index,param);
        }};
    }
}
}
