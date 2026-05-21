#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include "ClibUtil/editorID.hpp"
#include <nlohmann/json.hpp> 
#include <mutex>
#include <unordered_set>

namespace FormUtil {
    const RE::TESFile* GetMasterFile(RE::TESForm* ref);
    std::string NormalizeFormID(RE::TESForm* form);
    RE::FormID FormIDFromString(const std::string& str);
}

struct InternalFormInfo {
    RE::FormID formID;
    std::string editorID;
    std::string name;
    std::string pluginName;
    std::string formType;
    std::string cachedDisplayName;

    void UpdateDisplayName() {
        std::string base = !name.empty() ? name : (!editorID.empty() ? editorID : "Unknown");
        cachedDisplayName = std::format("{} [{:08X}]", base, formID);
    }
    // Helper for UI
    std::string GetDisplayName() const {
        if (!name.empty()) return name;
        if (!editorID.empty()) return editorID;
        return std::to_string(formID);
    }
};


class Manager {
public:
    static Manager* GetSingleton() {
        static Manager singleton;
        return &singleton;
    }

    void PopulateAllLists();


    static std::string ToUTF8(std::string_view a_str);
    const std::vector<InternalFormInfo>& GetList(const std::string& typeName);
    void RegisterReadyCallback(std::function<void()> callback);
    const InternalFormInfo* GetInfoByID(const std::string& type, RE::FormID id);
private:
    Manager() = default;
    
    template <typename T>
    void PopulateList(const std::string& a_typeName, std::function<bool(T*)> a_filter = nullptr);

    bool _isPopulated = false;
    std::map<std::string, std::vector<InternalFormInfo>> _dataStore;
    std::vector<std::function<void()>> _readyCallbacks;
};
