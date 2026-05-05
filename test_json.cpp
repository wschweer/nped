#include <iostream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
int main() {
    json j = {{"name", nullptr}};
    if (!j.is_object() || !j.contains("name") || !j["name"].is_string()) {
        std::cout << "Valid string check failed. " << j["name"].is_string() << std::endl;
        return 0;
    }
    std::string s = j.value("name", "");
    std::cout << "Success: " << s << std::endl;
    return 0;
}
