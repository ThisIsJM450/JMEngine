#pragma once
#include <string>

class Object
{
public:
    std::string GetName() const { return PrivateName; }
    void SetName(const std::string& InName) { PrivateName = InName; }
private:
    std::string PrivateName = std::string("None");
};
