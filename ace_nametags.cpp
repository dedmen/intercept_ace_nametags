/*
 * ace_nametags.cpp
 * Author: esteldunedain
 * Implements nametags using Intercept
 *
 * Takes:
 * None
 *
 * Returns:
 * None
 */
#include <Windows.h>
#include <stdio.h>
#include <cstdint>
#include <atomic>
#include <memory>

#include "intercept.hpp"
#include "client\client.hpp"
#include "client\pointers.hpp"
#include "client\sqf\uncategorized.hpp"
#include "client\sqf\config.hpp"

#include "nametagger.hpp"

using namespace intercept;
using namespace ace::nametags;

nametagger tagger;

int __cdecl intercept::api_version() {
    return 1;
}

void __cdecl intercept::on_frame() {
    tagger.on_frame();
}


void __cdecl intercept::post_init() {

}

void __cdecl intercept::mission_stopped() {

}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
    )
{
    //switch (ul_reason_for_call)
    //{
    //case DLL_PROCESS_ATTACH:
    //case DLL_THREAD_ATTACH:
    //case DLL_THREAD_DETACH:
    //case DLL_PROCESS_DETACH:
    //}
    return TRUE;
}
