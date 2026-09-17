#pragma once
#include "api.hpp"
#include "bridge.hpp"
#include <map>
#include <set>
#include <cmath>

namespace rmcp {
inline std::string text(const json& p, const char* key, size_t maximum = 4096) {
    if (!p.contains(key) || !p.at(key).is_string()) throw Error("INVALID_PARAMETER", std::string(key)+" must be text");
    auto value = p.at(key).get<std::string>();
    if (value.empty() || value.size() > maximum || value.find('\0') != std::string::npos)
        throw Error("INVALID_PARAMETER", std::string(key)+" has invalid length or NUL");
    return value;
}
inline double number(const json& p, const char* key, double low, double high) {
    if (!p.contains(key) || !p.at(key).is_number()) throw Error("INVALID_PARAMETER", std::string(key)+" must be a number");
    double value = p.at(key).get<double>();
    if (!std::isfinite(value) || value < low || value > high) throw Error("INVALID_PARAMETER", std::string(key)+" out of range");
    return value;
}
inline int integer(const json& p, const char* key, int low, int high) {
    if (!p.contains(key) || !p.at(key).is_number_integer()) throw Error("INVALID_PARAMETER", std::string(key)+" must be an integer");
    return static_cast<int>(number(p,key,low,high));
}
inline std::map<ReaProject*,std::string> project_ids;
inline unsigned long long next_project = 0;
inline void refresh_projects() {
    std::set<ReaProject*> live;
    for (int i=0; auto* p = EnumProjects(i,nullptr,0); ++i) {
        live.insert(p);
        if (!project_ids.count(p)) project_ids[p] = std::to_string(GetCurrentProcessId())+"-"+std::to_string(++next_project);
    }
    for (auto it=project_ids.begin();it!=project_ids.end();)
        if (!live.count(it->first)) it=project_ids.erase(it); else ++it;
}
inline ReaProject* project(const json& p) {
    refresh_projects();
    auto* current = EnumProjects(-1,nullptr,0);
    if (!current) throw Error("PROJECT_NOT_OPEN", "No active project");
    if (text(p,"project_id",128) != project_ids.at(current))
        throw Error("PROJECT_CHANGED", "Active project changed; inspect again");
    return current;
}
inline json project_info(ReaProject* proj, int index) {
    char name[4096]{};
    GetProjectName(proj,name,sizeof(name));
    char path[32768]{};
    EnumProjects(index,path,sizeof(path));
    return {{"project_id",project_ids.at(proj)},{"name",name},{"path",path},
        {"track_count",CountTracks(proj)},{"state_version",GetProjectStateChangeCount(proj)},
        {"dirty",IsProjectDirty(proj)!=0}};
}
struct Operation {
    std::string access;
    bool undo;
    std::function<json(const json&)> run;
};
inline std::map<std::string,Operation> operations;
inline void add_project_operations() {
    operations["project.get"] = {"read",false,[](const json&) {
        refresh_projects();
        auto* p = EnumProjects(-1,nullptr,0);
        if (!p) throw Error("PROJECT_NOT_OPEN", "No active project");
        return project_info(p,-1);
    }};
    operations["project.list"] = {"read",false,[](const json&) {
        refresh_projects(); json result = json::array();
        for (int i=0;auto* p=EnumProjects(i,nullptr,0);++i) result.push_back(project_info(p,i));
        return result;
    }};
    operations["project.save"] = {"destructive",false,[](const json& p) {
        auto* proj=project(p);
        char path[32768]{}; EnumProjects(-1,path,sizeof(path));
        if (!path[0]) throw Error("UNSAVED_PROJECT", "Save the project once in REAPER before using this tool");
        Main_SaveProject(proj,false);
        if (IsProjectDirty(proj)) throw Error("SAVE_FAILED", "REAPER still reports unsaved changes");
        return json{{"saved",true}};
    }};
    operations["project.undo"] = {"destructive",false,[](const json& p) {
        auto* proj=project(p);
        if (!Undo_CanUndo2(proj)) throw Error("NOTHING_TO_UNDO", "Undo history is empty");
        return json{{"changed",Undo_DoUndo2(proj)!=0}};
    }};
    operations["project.redo"] = {"destructive",false,[](const json& p) {
        auto* proj=project(p);
        if (!Undo_CanRedo2(proj)) throw Error("NOTHING_TO_REDO", "Redo history is empty");
        return json{{"changed",Undo_DoRedo2(proj)!=0}};
    }};
}
struct Undo {
    ReaProject* proj; std::string label;
    Undo(ReaProject* p,const std::string& method):proj(p),label("REAPER MCP: "+method) { Undo_BeginBlock2(proj); }
    ~Undo() { Undo_EndBlock2(proj,label.c_str(),-1); UpdateArrange(); }
};
inline json dispatch(const std::string& method,const json& p,const std::string& policy) {
    if (method=="bridge.info") {
        json caps=json::array(); for (auto& op:operations) caps.push_back(op.first);
        return {{"protocol_version",1},{"extension_version","0.1.0a1"},
            {"reaper_version",GetAppVersion()},{"policy",policy},{"capabilities",caps}};
    }
    auto it=operations.find(method);
    if (it==operations.end()) throw Error("METHOD_NOT_FOUND", "Unknown method");
    auto& op=it->second;
    if (policy=="read-only" && op.access!="read") throw Error("PERMISSION_DENIED", "Extension is read-only");
    if ((op.access=="destructive" || op.access=="render") && policy!="full-control" && !p.value("confirm",false))
        throw Error("CONFIRMATION_REQUIRED", "Obtain user approval and pass confirm=true");
    if (op.undo) { Undo undo(project(p),method); return op.run(p); }
    return op.run(p);
}
}
