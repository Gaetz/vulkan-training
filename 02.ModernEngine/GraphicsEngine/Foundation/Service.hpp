#pragma once

#include "Platform.hpp"

struct Service {

    virtual void Init(void* configuration) { }
    virtual void Shutdown() { }

}; // struct Service

#define G_DECLARE_SERVICE(Type) static Type* Instance();

