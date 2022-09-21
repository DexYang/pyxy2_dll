// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include "mapx.h"
#include "pyxy2.h"
#include<iostream>

#define EXPORT __declspec(dllexport)

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}


PyXY2 pyxy2 = PyXY2();

extern "C" {
    EXPORT uint8_t* load(char* filename, int index) {
        MapX map = MapX(filename, 0);
        map.ReadJPEG(index);
        std::set<int> masks = map.GetBlockMasks(index);
        if (masks.size() > 0) {
            map.ReadMask(*masks.begin());
        }
        return map.GetJPEGRGB(index);
    }

    EXPORT void loop() {
        pyxy2.Loop();
    }

    EXPORT void end_loop() {
        pyxy2.EndLoop();
    }

    EXPORT void set_direction(int cd, int sd) {
        pyxy2.SetDirection(cd, sd);
    }

    EXPORT void add_task(char* filename, int index, int type) {
        PyXY2::Task task = PyXY2::Task(filename, index, type);
        pyxy2.AddTask(task);
    }

    EXPORT PyObject* get_map_info_py(char* filename) {
        return pyxy2.GetMapInfoPy(filename);
    }

    EXPORT uint32_t* get_map_cell(char* filename) {
        return pyxy2.GetMapCell(filename);
    }

    EXPORT void drop_map(char* filename) {
        pyxy2.DropMap(filename);
    }

    // JPEG

    EXPORT MapX::MapBlock* get_block_info(char* filename, int index) {
        return pyxy2.GetBlockInfo(filename, index);
    }

    EXPORT PyObject* get_block_masks_py(char* filename, int index) {
        return pyxy2.GetBlockMasksPy(filename, index);
    }

    EXPORT std::set<int> get_block_masks(char* filename, int index) {
        return pyxy2.GetBlockMasks(filename, index);
    }

    EXPORT void read_jpeg(char* filename, int index) {
        pyxy2.ReadJPEG(filename, index);
    }

    EXPORT bool has_jpeg_loaded(char* filename, int index) {
        return pyxy2.HasJPEGLoaded(filename, index);
    }

    EXPORT uint8_t* get_jpeg_rgb(char* filename, int index) {
        return pyxy2.GetJPEGRGB(filename, index);
    }

    EXPORT void erase_jpeg_rgb(char* filename, int index) {
        pyxy2.EraseJPEGRGB(filename, index);
    }

    // Mask

    EXPORT MapX::MaskInfo* get_mask_info(char* filename, int index) {
        return pyxy2.GetMaskInfo(filename, index);
    }

    EXPORT PyObject* get_mask_info_py(char* filename, int index) {
        return pyxy2.GetMaskInfoPy(filename, index);
    }

    EXPORT void read_mask(char* filename, int index) {
        return pyxy2.ReadMask(filename, index);
    }

    EXPORT bool has_mask_loaded(char* filename, int index) {
        return pyxy2.HasMaskLoaded(filename, index);
    }

    EXPORT uint8_t* get_mask_rgba(char* filename, int index) {
        return pyxy2.GetMaskRGBA(filename, index);
    }

    EXPORT void erase_mask_rgb(char* filename, int index) {
        pyxy2.EraseMaskRGB(filename, index);
    }

    //读取WAS调色板
    EXPORT unsigned char* read_pal(unsigned char* lp) {
        unsigned char* tmp = (unsigned char*)pyxy2.read_color_pal(lp);
        return tmp;
    }

    //解析WAS帧图片
    EXPORT unsigned char* read_frame(unsigned char* lp, unsigned char* Palette32) {
        unsigned char* tmp = pyxy2.read_frame(lp, (PyXY2::_PIXEL*)Palette32);
        return tmp;
    }

    EXPORT void delete_me(char* ptr) {
        delete ptr;
    }

    EXPORT PyObject* get_hash_py(char* s) {
        return pyxy2.GetHashPy(s);
    }
}
