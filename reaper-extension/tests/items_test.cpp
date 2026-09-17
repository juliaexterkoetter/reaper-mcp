#include "bridge.hpp"
#define REAPERAPI_IMPLEMENT
#include "items.hpp"
#include <cstring>
#include <iostream>
#include <memory>
struct FakeItem { double pos,len,offset; bool alive=true,locked=false; std::string guid; };
std::vector<std::unique_ptr<FakeItem>> media;
int splits=0;
int main() {
    using namespace rmcp;
    media.push_back(std::make_unique<FakeItem>(FakeItem{0,10,3,true,false,"{A}"}));
    EnumProjects=[](int i,char*,int)->ReaProject*{return i==0||i==-1?reinterpret_cast<ReaProject*>(1):nullptr;};
    CountMediaItems=[](ReaProject*){return static_cast<int>(media.size());};
    GetMediaItem=[](ReaProject*,int i)->MediaItem*{return media[i]->alive?reinterpret_cast<MediaItem*>(media[i].get()):nullptr;};
    GetSetMediaItemInfo_String=[](MediaItem* p,const char*,char* out,bool){std::strcpy(out,reinterpret_cast<FakeItem*>(p)->guid.c_str());return true;};
    GetMediaItemInfo_Value=[](MediaItem* p,const char* key){auto* it=reinterpret_cast<FakeItem*>(p);std::string k=key;
        if(k=="D_POSITION")return it->pos;if(k=="D_LENGTH")return it->len;if(k=="D_VOL")return 1.0;if(k=="C_LOCK")return it->locked?1.0:0.0;return 0.0;};
    GetMediaItemTrack=[](MediaItem*)->MediaTrack*{return reinterpret_cast<MediaTrack*>(1);};
    GetSetMediaTrackInfo_String=[](MediaTrack*,const char*,char* out,bool){std::strcpy(out,"{TRACK}");return true;};
    SplitMediaItem=[](MediaItem* p,double at)->MediaItem*{
        auto* left=reinterpret_cast<FakeItem*>(p);++splits;
        auto right=std::make_unique<FakeItem>(FakeItem{at,left->pos+left->len-at,left->offset+(at-left->pos)*2,true,false,"{"+std::to_string(splits)+"}"});
        left->len=at-left->pos;auto* raw=right.get();media.push_back(std::move(right));return reinterpret_cast<MediaItem*>(raw);};
    DeleteTrackMediaItem=[](MediaTrack*,MediaItem* p){reinterpret_cast<FakeItem*>(p)->alive=false;return true;};
    Undo_BeginBlock2=[](ReaProject*){};Undo_EndBlock2=[](ReaProject*,const char*,int){};UpdateArrange=[](){};
    refresh_projects();add_item_operations();
    json args={{"project_id",project_ids.begin()->second},{"item","{A}"},{"start_seconds",5},{"end_seconds",8},{"confirm",true}};
    try {
        args["end_seconds"]=11;
        try{dispatch("items.trim",args,"confirm-destructive");throw std::runtime_error("bad interval accepted");}
        catch(const Error& e){if(e.code!="INVALID_PARAMETER" || splits)throw;}
        args["end_seconds"]=8;auto result=dispatch("items.trim",args,"confirm-destructive");
        if(result.at("position_seconds")!=5 || result.at("length_seconds")!=3 || splits!=2 || media.back()->offset!=13)
            throw std::runtime_error("trim semantics failed");
        media.back()->locked=true;args["item"]=result.at("guid");
        try{dispatch("items.delete",args,"confirm-destructive");throw std::runtime_error("locked item deleted");}
        catch(const Error& e){if(e.code!="ITEM_LOCKED")throw;}
        std::cout<<"Item trim and lock checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
