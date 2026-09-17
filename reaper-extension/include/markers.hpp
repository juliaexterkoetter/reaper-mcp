#pragma once
#include "fx.hpp"
namespace rmcp {
inline json marker_info(ReaProject* proj,ProjectMarker* mark) {
    auto* guid=static_cast<GUID*>(GetSetRegionOrMarkerInfo(proj,mark,"GUID",nullptr));
    auto* name=static_cast<const char*>(GetSetRegionOrMarkerInfo(proj,mark,"P_NAME",nullptr));
    return {{"guid",guid_string(guid)},{"name",name?name:""},
        {"position_seconds",GetRegionOrMarkerInfo_Value(proj,mark,"D_STARTPOS")},
        {"end_seconds",GetRegionOrMarkerInfo_Value(proj,mark,"D_ENDPOS")},
        {"number",GetRegionOrMarkerInfo_Value(proj,mark,"I_NUMBER")}};
}
inline ProjectMarker* marker(ReaProject* proj,const json& p,bool region) {
    auto guid=text(p,"guid",128);auto* mark=GetRegionOrMarker(proj,-1,guid.c_str());
    if(!mark || (GetRegionOrMarkerInfo_Value(proj,mark,"B_ISREGION")!=0)!=region)
        throw Error("MARKER_NOT_FOUND","Marker or region not found");
    return mark;
}
inline void add_marker_operations() {
    for(bool region:{false,true}) {std::string prefix=region?"regions.":"markers.";
        operations[prefix+"list"]={"read",false,[region](const json& p){auto* proj=project(p);json result=json::array();
            int count=GetNumRegionsOrMarkers(proj);
            if(count>10000)throw Error("RESULT_LIMIT","Project exceeds 10000 markers/regions");
            for(int i=0;i<count;++i){auto* m=GetRegionOrMarker(proj,i,nullptr);
                if(m && (GetRegionOrMarkerInfo_Value(proj,m,"B_ISREGION")!=0)==region)result.push_back(marker_info(proj,m));}
            return result;}};
        for(bool update:{false,true}) {
            operations[prefix+(update?"update":"create")]={"write",true,[region,update](const json& p){
                auto* proj=project(p);auto name=text(p,"name",1024);double start=number(p,"position_seconds",0,8640000);
                double end=region?number(p,"end_seconds",0,8640000):start;
                if(region && end<=start)throw Error("INVALID_PARAMETER","Region end must follow start");
                ProjectMarker* m=nullptr;
                if(update){m=marker(proj,p,region);
                    // Set end first when moving right; start first when moving left, avoiding inverted intermediate ranges.
                    if(start>GetRegionOrMarkerInfo_Value(proj,m,"D_STARTPOS")){
                        if(region)SetRegionOrMarkerInfo_Value(proj,m,"D_ENDPOS",end);
                        SetRegionOrMarkerInfo_Value(proj,m,"D_STARTPOS",start);
                    }else{SetRegionOrMarkerInfo_Value(proj,m,"D_STARTPOS",start);if(region)SetRegionOrMarkerInfo_Value(proj,m,"D_ENDPOS",end);}
                    if(!GetSetRegionOrMarkerInfo_String(proj,m,"P_NAME",name.data(),true))throw Error("EDIT_FAILED","Marker rename failed; inspect state");
                }else m=AddRegionOrMarker(proj,region,start,end,name.c_str(),-1,0);
                if(!m)throw Error("EDIT_FAILED","Marker creation failed");return marker_info(proj,m);
            }};
        }
        operations[prefix+"delete"]={"destructive",true,[region](const json& p){auto* proj=project(p);auto* m=marker(proj,p,region);
            int index=static_cast<int>(GetRegionOrMarkerInfo_Value(proj,m,"I_INDEX"));
            if(!DeleteProjectMarkerByIndex(proj,index))throw Error("EDIT_FAILED","Marker deletion failed");return json{{"deleted",true}};}};
    }
}
}
