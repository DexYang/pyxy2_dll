#pragma once
#pragma once
#include "pch.h"
#include "mapx.h"
#include <queue>
#include <unordered_map>
#include <string>
#include <vector> 
#include<iostream>
#include <Python.h>



class PyXY2 {
public:
	struct Task {
		std::string filename;
		int type;
		int index;
		Task(char* filename, int index, int type)
			:filename(filename),
			index(index),
			type(type)
		{

		}
	};

	struct _PIXEL {
		unsigned char R; // 红色
		unsigned char G; // 绿色
		unsigned char B; // 蓝色
		unsigned char A; // 通道
	};


	// 帧的文件头
	struct FRAME
	{
		unsigned int Key_X;			// 图片的关键位X
		unsigned int Key_Y;			// 图片的关键位Y
		unsigned int Width;			// 图片的宽度，单位像素
		unsigned int Height;		// 图片的高度，单位像素
	};

	std::queue<Task> TaskQueue;

	std::unordered_map<std::string, std::shared_ptr<MapX>> MapPool;

	PyXY2();

	~PyXY2();

	void AddTask(Task task);

	void AddMap(std::string filename);

	bool HasMap(std::string filename);

	std::shared_ptr<MapX> GetMap(std::string filename);

	void DropMap(std::string filename);

	PyObject* GetMapInfoPy(std::string filename);

	uint32_t* GetMapCell(std::string filename) {
		return GetMap(filename)->GetCell();
	}

	// JPEG

	MapX::MapBlock* GetBlockInfo(std::string filename, int index) {
		return GetMap(filename)->GetBlockInfo(index);
	}

	std::set<int> GetBlockMasks(std::string filename, int index) {
		return GetMap(filename)->GetBlockMasks(index);
	}

	PyObject* GetBlockMasksPy(std::string filename, int index);

	void ReadJPEG(std::string filename, int index) {
		GetMap(filename)->ReadJPEG(index);
	}

	bool HasJPEGLoaded(std::string filename, int index) {
		return GetMap(filename)->HasJPEGLoaded(index);
	}

	uint8_t* GetJPEGRGB(std::string filename, int index) {
		return GetMap(filename)->GetJPEGRGB(index);
	}

	void EraseJPEGRGB(std::string filename, int index) {
		GetMap(filename)->EraseJPEGRGB(index);
	}

	// Mask

	MapX::MaskInfo* GetMaskInfo(std::string filename, int index) {
		return GetMap(filename)->GetMaskInfo(index);
	}

	PyObject* GetMaskInfoPy(std::string filename, int index);

	void ReadMask(std::string filename, int index) {
		GetMap(filename)->ReadMask(index);
	}

	void ReadMaskOrigin(std::string filename, int index) {
		GetMap(filename)->ReadMaskOrigin(index);
	}

	bool HasMaskLoaded(std::string filename, int index) {
		return GetMap(filename)->HasMaskLoaded(index);
	}

	uint8_t* GetMaskRGBA(std::string filename, int index) {
		return GetMap(filename)->GetMaskRGBA(index);
	}

	void EraseMaskRGB(std::string filename, int index) {
		GetMap(filename)->EraseMaskRGB(index);
	}

	void SetDirection(int cd, int sd) {
		m_CoordinateDirection = cd;
		m_ScanDirection = sd;
	}

	// Loop

	int m_CoordinateDirection;

	int m_ScanDirection;

	void Loop();

	void EndLoop();

	bool running = false;

	bool HasTask();

	Task GetTask();

	// WDF  WAS

	char* StringAdjust(char* p);

	unsigned long StringId(const char* str);

	PyObject* GetHashPy(char* p);

	unsigned char* read_color_pal(unsigned char* lp);

	unsigned char* read_frame(unsigned char* lp, _PIXEL* Palette32);

	_PIXEL RGB565to888(unsigned short color, unsigned char Alpha);

	_PIXEL SetAlpha(_PIXEL Color, uint8_t Alpha) {
		Color.A = Alpha;
		return Color;
	};
};