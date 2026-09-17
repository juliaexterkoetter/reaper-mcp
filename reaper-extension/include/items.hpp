#pragma once
#include "tracks.hpp"
namespace rmcp {
inline std::string item_guid(MediaItem* item) {
    char out[128]{};
    if(!GetSetMediaItemInfo_String(item,"GUID",out,false)) throw Error("INVALID_ITEM","Cannot read item GUID");
    return out;
}
inline MediaItem* item(const json& p,bool editing=false) {
    auto* proj=project(p); auto id=text(p,"item",128);
    for(int i=0;i<CountMediaItems(proj);++i) {
        auto* it=GetMediaItem(proj,i);
        if(it && item_guid(it)==id) {
            if(editing && (static_cast<int>(GetMediaItemInfo_Value(it,"C_LOCK"))&1)) throw Error("ITEM_LOCKED","Unlock the item before editing");
            return it;
        }
    }
    throw Error("ITEM_NOT_FOUND","Item not found; inspect again");
}
inline json item_info(MediaItem* it) {
    return {{"guid",item_guid(it)},{"track_guid",track_guid(GetMediaItemTrack(it))},
        {"position_seconds",GetMediaItemInfo_Value(it,"D_POSITION")},
        {"length_seconds",GetMediaItemInfo_Value(it,"D_LENGTH")},
        {"volume_db",gain_db(GetMediaItemInfo_Value(it,"D_VOL"))},
        {"fade_in_seconds",GetMediaItemInfo_Value(it,"D_FADEINLEN")},
        {"fade_out_seconds",GetMediaItemInfo_Value(it,"D_FADEOUTLEN")}};
}
inline void set_item(MediaItem* it,const char* key,double value) {
    if(!SetMediaItemInfo_Value(it,key,value)) throw Error("EDIT_FAILED","Item property was not changed");
}
inline void delete_item(MediaItem* it) {
    if(!DeleteTrackMediaItem(GetMediaItemTrack(it),it)) throw Error("EDIT_FAILED","Item deletion failed; inspect state");
}
inline void add_item_operations() {
    operations["items.list"]={"read",false,[](const json& p){
        auto* proj=project(p); int count=CountMediaItems(proj);
        int offset=p.contains("offset")?integer(p,"offset",0,10000000):0;
        int limit=p.contains("limit")?integer(p,"limit",1,1000):200; json result=json::array();
        for(int i=offset;i<std::min(count,offset+limit);++i) result.push_back(item_info(GetMediaItem(proj,i)));
        return json{{"items",result},{"total",count},{"offset",offset}};
    }};
    operations["items.get"]={"read",false,[](const json& p){return item_info(item(p));}};
    operations["items.move"]={"write",true,[](const json& p){auto* it=item(p,true);
        set_item(it,"D_POSITION",number(p,"position_seconds",0,8640000));return item_info(it);}};
    operations["items.split"]={"write",true,[](const json& p){
        auto* it=item(p,true); double pos=number(p,"position_seconds",0,8640000);
        double start=GetMediaItemInfo_Value(it,"D_POSITION"), end=start+GetMediaItemInfo_Value(it,"D_LENGTH");
        if(pos<=start || pos>=end) throw Error("INVALID_PARAMETER","Split must be strictly inside the item");
        auto* right=SplitMediaItem(it,pos);
        if(!right) throw Error("EDIT_FAILED","REAPER could not split this item");
        return json{{"left",item_info(it)},{"right",item_info(right)}};
    }};
    operations["items.trim"]={"destructive",true,[](const json& p){
        auto* it=item(p,true); double start=number(p,"start_seconds",0,8640000),end=number(p,"end_seconds",0,8640000);
        double old_start=GetMediaItemInfo_Value(it,"D_POSITION"),old_end=old_start+GetMediaItemInfo_Value(it,"D_LENGTH");
        if(start<old_start || end>old_end || start>=end) throw Error("INVALID_PARAMETER","Trim interval must lie within the item");
        if(end<old_end) {
            auto* tail=SplitMediaItem(it,end);
            if(!tail) throw Error("EDIT_FAILED","End split failed");
            delete_item(tail);
        }
        if(start>old_start) {
            auto* keep=SplitMediaItem(it,start);
            if(!keep) throw Error("EDIT_FAILED","Start split failed after end trim; use Undo");
            delete_item(it); it=keep;
        }
        return item_info(it);
    }};
    operations["items.delete"]={"destructive",true,[](const json& p){delete_item(item(p,true));return json{{"deleted",true}};}};
    operations["items.volume"]={"write",true,[](const json& p){auto* it=item(p,true);
        set_item(it,"D_VOL",volume(p,GetMediaItemInfo_Value(it,"D_VOL")));return item_info(it);}};
    for(auto key:{"in","out"}) { std::string direction=key;
        operations["items.fade_"+direction]={"write",true,[direction](const json& p){
            auto* it=item(p,true); double seconds=number(p,"seconds",0,GetMediaItemInfo_Value(it,"D_LENGTH"));
            set_item(it,direction=="in"?"D_FADEINLEN_AUTO":"D_FADEOUTLEN_AUTO",-1);
            set_item(it,direction=="in"?"D_FADEINLEN":"D_FADEOUTLEN",seconds); return item_info(it);
        }};
    }
}
}
