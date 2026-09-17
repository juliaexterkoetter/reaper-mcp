#pragma once
#include "adapter.hpp"
namespace rmcp {
inline json play_state(ReaProject* proj) {
    int state=GetPlayStateEx(proj);
    return {{"playing",(state&1)!=0},{"paused",(state&2)!=0},{"recording",(state&4)!=0}};
}
inline void add_transport_operations() {
    operations["transport.state"]={"read",false,[](const json& p){return play_state(project(p));}};
    operations["transport.cursor"]={"read",false,[](const json& p){return json{{"position_seconds",GetCursorPositionEx(project(p))}};}};
    operations["transport.set_cursor"]={"write",false,[](const json& p){
        auto* proj=project(p);double pos=number(p,"position_seconds",0,8640000);
        SetEditCurPos2(proj,pos,false,p.value("seek_playback",false));return json{{"position_seconds",GetCursorPositionEx(proj)}};
    }};
    for(auto action:{"play","stop","pause"}) {std::string a=action;
        operations["transport."+a]={"write",false,[a](const json& p){auto* proj=project(p);
            if(a=="play") OnPlayButtonEx(proj);
            else if(a=="stop") OnStopButtonEx(proj);
            else if(!(GetPlayStateEx(proj)&2)) OnPauseButtonEx(proj);
            return play_state(proj);
        }};
    }
}
}
