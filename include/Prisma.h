#pragma once
#include <nlohmann/json.hpp> 
#include <unordered_map>
#include <unordered_set>

class Prisma {
    static inline bool createdView = false;
public:
    static void Install();
    static void Show();
    static void Hide();
    static bool IsHidden();
    static void SendKeyPress(const std::string& key);
    static inline bool TPM = false;
};