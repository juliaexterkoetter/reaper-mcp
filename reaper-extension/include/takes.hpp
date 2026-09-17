#pragma once
#include "items.hpp"
namespace rmcp {
inline json take_info(MediaItem_Take* take) {
    if(!take) return nullptr;
    char guid[128]{},name[4096]{};
    if(!GetSetMediaItemTakeInfo_String(take,"GUID",guid,false)) throw Error("INVALID_TAKE","Cannot read take GUID");
    GetSetMediaItemTakeInfo_String(take,"P_NAME",name,false);
    return {{"guid",guid},{"name",name},{"source_offset_seconds",GetMediaItemTakeInfo_Value(take,"D_STARTOFFS")},
        {"playback_rate",GetMediaItemTakeInfo_Value(take,"D_PLAYRATE")},{"is_midi",TakeIsMIDI(take)}};
}
inline void add_take_operations() {
    operations["takes.list"]={"read",false,[](const json& p){
        auto* it=item(p);json result=json::array();int count=CountTakes(it);
        if(count>1000) throw Error("RESULT_LIMIT","Item exceeds 1000 takes");
        for(int i=0;i<count;++i) result.push_back(take_info(GetTake(it,i)));return result;
    }};
    operations["takes.active"]={"read",false,[](const json& p){return take_info(GetActiveTake(item(p)));}};
}
}
