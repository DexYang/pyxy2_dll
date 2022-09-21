#include "pch.h"
#include "pyxy2.h"
#include <chrono>
using std::ios;

PyXY2::PyXY2() {
	std::cerr << "*********Init Res Decoder*********" << std::endl;
}

PyXY2::~PyXY2() {

}

void PyXY2::AddTask(Task task) {
	TaskQueue.push(task);
}

void PyXY2::AddMap(std::string filename) {
	if (HasMap(filename)) {
		return;
	}
	auto map = std::make_shared<MapX>(filename, m_ScanDirection);
	MapPool[filename] = map;
};

bool PyXY2::HasMap(std::string filename) {
	return MapPool.count(filename) != 0;
};

std::shared_ptr<MapX> PyXY2::GetMap(std::string filename) {
	if (!HasMap(filename)) {
		AddMap(filename);
	}
	return MapPool.at(filename);
}

void PyXY2::DropMap(std::string filename) {
	if (HasMap(filename)) {
		MapPool.erase(filename);
	}
}

bool PyXY2::HasTask() {
	return TaskQueue.size() > 0;
}

PyXY2::Task PyXY2::GetTask() {
	Task task = TaskQueue.front();
	TaskQueue.pop();
	return task;
}

void PyXY2::Loop() {
	running = true;
	while (running) {
		if (HasTask()) {
			Task task = GetTask();
			if (task.type == 0) {
				ReadJPEG(task.filename, task.index);
			}
			else if (task.type == 1) {
				ReadMask(task.filename, task.index);
			}
			else if (task.type == 2) {
				ReadMaskOrigin(task.filename, task.index);
			}
		}
		else {
			Sleep(100);
		}
	}
}

void PyXY2::EndLoop() {
	running = false;
}

PyObject* PyXY2::GetMapInfoPy(std::string filename) {
	PyGILState_STATE gstate = PyGILState_Ensure();

	PyObject* result = PyDict_New();
	std::shared_ptr<MapX> mapx = GetMap(filename);

	PyDict_SetItem(result, Py_BuildValue("s", "height"), Py_BuildValue("i", mapx->GetHeight()));

	PyDict_SetItem(result, Py_BuildValue("s", "width"), Py_BuildValue("i", mapx->GetWidth()));

	PyDict_SetItem(result, Py_BuildValue("s", "col_count"), Py_BuildValue("i", mapx->GetColCount()));

	PyDict_SetItem(result, Py_BuildValue("s", "row_count"), Py_BuildValue("i", mapx->GetRowCount()));

	PyDict_SetItem(result, Py_BuildValue("s", "cell_col_count"), Py_BuildValue("i", mapx->GetCellColCount()));

	PyDict_SetItem(result, Py_BuildValue("s", "cell_row_count"), Py_BuildValue("i", mapx->GetCellRowCount()));

	PyDict_SetItem(result, Py_BuildValue("s", "mask_count"), Py_BuildValue("i", mapx->GetMaskCount()));

	PyGILState_Release(gstate);
	return result;
}

PyObject* PyXY2::GetBlockMasksPy(std::string filename, int index) {
	std::set<int> masks = GetBlockMasks(filename, index);
	PyGILState_STATE gstate = PyGILState_Ensure();
	PyObject* result = PyList_New(0);
	for (std::set<int>::iterator it = masks.begin(); it != masks.end(); ++it) {
		PyList_Append(result, Py_BuildValue("i", *it));
	}
	PyGILState_Release(gstate);
	return result;
}

PyObject* PyXY2::GetMaskInfoPy(std::string filename, int index) {
	PyGILState_STATE gstate = PyGILState_Ensure();

	PyObject* result = PyDict_New();
	MapX::MaskInfo* masks = GetMaskInfo(filename, index);

	PyDict_SetItem(result, Py_BuildValue("s", "id"), Py_BuildValue("i", masks->id));

	PyDict_SetItem(result, Py_BuildValue("s", "start_x"), Py_BuildValue("i", masks->StartX));

	int maskStartY = m_CoordinateDirection == 0 ? masks->StartY : GetMap(filename)->GetHeight() - masks->StartY;
	PyDict_SetItem(result, Py_BuildValue("s", "start_y"), Py_BuildValue("i", maskStartY));

	PyDict_SetItem(result, Py_BuildValue("s", "width"), Py_BuildValue("i", masks->Width));

	PyDict_SetItem(result, Py_BuildValue("s", "height"), Py_BuildValue("i", masks->Height));

	PyGILState_Release(gstate);
	return result;
}

char* PyXY2::StringAdjust(char* p) {
	int i;
	for (i = 0; p[i]; i++) {
		if (p[i] >= 'A' && p[i] <= 'Z') p[i] += 'a' - 'A';
		else if (p[i] == '/') p[i] = '\\';
	}
	return p;
}

unsigned long PyXY2::StringId(const char* str) {
	int i;
	unsigned int v;
	static unsigned m[70];
	strncpy((char*)m, str, 256);
	for (i = 0; i < 256 / 4 && m[i]; i++);
	m[i++] = 0x9BE74448, m[i++] = 0x66F42C48;
	v = 0xF4FA8928;

	unsigned int cf = 0;
	unsigned int esi = 0x37A8470E;
	unsigned int edi = 0x7758B42B;
	unsigned int eax = 0;
	unsigned int ebx = 0;
	unsigned int ecx = 0;
	unsigned int edx = 0;
	unsigned long long temp = 0;
	while (true) {
		// mov ebx, 0x267B0B11
		ebx = 0x267B0B11;
		// rol v, 1
		cf = (v & 0x80000000) > 0 ? 1 : 0;
		v = ((v << 1) & 0xFFFFFFFF) + cf;
		// xor ebx, v
		ebx = ebx ^ v;
		// mov eax, [eax + ecx * 4]
		eax = m[ecx];
		// mov edx, ebx
		edx = ebx;
		// xor esi, eax
		esi = esi ^ eax;
		// xor edi, eax
		edi = edi ^ eax;
		// add edx, edi
		temp = (unsigned long long)edx + (unsigned long long)edi;
		cf = (temp & 0x100000000) > 0 ? 1 : 0;
		edx = temp & 0xFFFFFFFF;
		// or edx, 0x2040801
		edx = edx | 0x2040801;
		// and edx, 0xBFEF7FDF
		edx = edx & 0xBFEF7FDF;
		// mov eax, esi
		eax = esi;
		// mul edx
		temp = (unsigned long long)eax * (unsigned long long)edx;
		eax = temp & 0xffffffff;
		edx = (temp >> 32) & 0xffffffff;
		cf = edx > 0 ? 1 : 0;
		// adc eax, edx
		temp = (unsigned long long)eax + (unsigned long long)edx + (unsigned long long)cf;
		eax = temp & 0xffffffff;
		cf = (temp & 0x100000000) > 0 ? 1 : 0;
		// mov edx, ebx
		edx = ebx;
		// adc eax, 0
		temp = (unsigned long long)eax + (unsigned long long)cf;
		eax = temp & 0xffffffff;
		cf = (temp & 0x100000000) > 0 ? 1 : 0;
		// add edx, esi
		temp = (unsigned long long)edx + (unsigned long long)esi;
		cf = (temp & 0x100000000) > 0 ? 1 : 0;
		edx = temp & 0xFFFFFFFF;
		// or edx, 0x804021
		edx = edx | 0x804021;
		// and edx, 0x7DFEFBFF
		edx = edx & 0x7DFEFBFF;
		// mov esi, eax
		esi = eax;
		// mov eax, edi
		eax = edi;
		// mul edx
		temp = (unsigned long long)eax * (unsigned long long)edx;
		eax = temp & 0xffffffff;
		edx = (temp >> 32) & 0xffffffff;
		cf = edx > 0 ? 1 : 0;
		// add edx, edx
		temp = (unsigned long long)edx + (unsigned long long)edx;
		cf = (temp & 0x100000000) > 0 ? 1 : 0;
		edx = temp & 0xFFFFFFFF;
		// adc eax, edx
		temp = (unsigned long long)eax + (unsigned long long)edx + (unsigned long long)cf;
		eax = temp & 0xffffffff;
		cf = (temp & 0x100000000) > 0 ? 1 : 0;
		// jnc _skip
		if (cf != 0) {
			// add eax, 2
			temp = (unsigned long long)eax + 2;
			cf = (temp & 0x100000000) > 0 ? 1 : 0;
			eax = temp & 0xFFFFFFFF;
		}
		// inc ecx;
		ecx += 1;
		// mov edi, eax
		edi = eax;
		// cmp ecx, i  jnz _loop
		if (ecx - i == 0) break;
	}
	// xor esi, edi
	esi = esi ^ edi;
	// mov v, esi
	v = esi;
	return v;
}

PyObject* PyXY2::GetHashPy(char* p) {
	unsigned long hash = StringId(StringAdjust(p));
	std::cout << hash << std::endl;
	PyObject* Hash = Py_BuildValue("I", hash);
	return Hash;
}


unsigned char* PyXY2::read_color_pal(unsigned char* lp) {
	unsigned short* short_lp = (unsigned short*)lp;
	_PIXEL* Palette32 = new _PIXEL[256]; // ����32bit��ɫ��Ŀռ�
	for (int k = 0; k < 256; k++) {
		Palette32[k] = RGB565to888(*short_lp++, 0xff); // 16to32��ɫ��ת��
	}
	return (unsigned char* )Palette32;
}

unsigned char* PyXY2::read_frame(unsigned char* lp, _PIXEL* pal) {
	unsigned int* int_lp = (unsigned int*)lp;
	FRAME frame;
	frame.Key_X = *int_lp++;
	frame.Key_Y = *int_lp++;
	frame.Width = *int_lp++;
	frame.Height = *int_lp++;

	unsigned int* FrameLineIndex = new unsigned int[frame.Height]; // ������ �����б�Ŀռ�
	for (unsigned int k = 0; k < frame.Height; k++) {
		FrameLineIndex[k] = *int_lp++;  // ͼƬ �� ����
	}

	unsigned int pixels = frame.Width * frame.Height; // ����������ֵ
	_PIXEL* BmpBuffer = new _PIXEL[pixels];
	unsigned char* BP = (unsigned char*)BmpBuffer;
	memset(BmpBuffer, 0, pixels * 4);

	for (uint32_t h = 0; h < frame.Height; h++) {
		uint32_t linePixels = 0;
		bool lineNotOver = true;
		uint8_t* pData = (uint8_t*)lp + FrameLineIndex[h];
		while (*pData != 0 && lineNotOver) {

			uint8_t level = 0;  // Alpha
			uint8_t repeat = 0; // �ظ�����
			_PIXEL color;	//�ظ���ɫ
			color.R = 0;
			color.G = 0;
			color.B = 0;
			color.A = 0;
			uint8_t style = (*pData & 0xc0) >> 6;   // ȡ�ֽڵ�ǰ��������
			switch (style) {
			case 0:   // {00******}
				if (*pData & 0x20) {  // {001*****} ��ʾ����Alphaͨ���ĵ�������
					// {001 +5bit Alpha}+{1Byte Index}, ��ʾ����Alphaͨ���ĵ������ء�
					// {001 +0~31��Alphaͨ��}+{1~255����ɫ������}
					level = (*pData) & 0x1f;  // 0x1f=(11111) ���Alphaͨ����ֵ
					if (*(pData - 1) == 0xc0) {  //���⴦��
						//Level = 31;
						//Pixels--;
						//pos--;
						if (linePixels <= frame.Width) {
							*BmpBuffer = *(BmpBuffer - 1);
							BmpBuffer++;
							linePixels++;
							pData += 2;
							break;
						}
						else {
							lineNotOver = false;
						}
					}
					pData++;  // ��һ���ֽ�
					if (linePixels <= frame.Width) {
						*BmpBuffer = SetAlpha(pal[*pData], (level << 3) | 7 - 1);
						BmpBuffer++;
						linePixels++;
						pData++;
					}
					else {
						lineNotOver = false;
					}
				}
				else {   // {000*****} ��ʾ�ظ�n�δ���Alphaͨ��������
				 // {000 +5bit Times}+{1Byte Alpha}+{1Byte Index}, ��ʾ�ظ�n�δ���Alphaͨ�������ء�
				 // {000 +�ظ�1~31��}+{0~255��Alphaͨ��}+{1~255����ɫ������}
				 // ע: �����{00000000} �����������н���ʹ�ã�����ֻ�����ظ�1~31�Ρ�
					repeat = (*pData) & 0x1f; // ����ظ��Ĵ���
					pData++;
					level = *pData; // ���Alphaͨ��ֵ
					pData++;
					color = SetAlpha(pal[*pData], (level << 3) | 7 - 1);
					for (int i = 1; i <= repeat; i++) {
						if (linePixels <= frame.Width) {
							*BmpBuffer = color;
							BmpBuffer++;
							linePixels++;
						}
						else {
							lineNotOver = false;
						}
					}
					pData++;
				}
				break;
			case 1: // {01******} ��ʾ����Alphaͨ�����ظ���n��������ɵ����ݶ�
			// {01  +6bit Times}+{nByte Datas},��ʾ����Alphaͨ�����ظ���n��������ɵ����ݶΡ�
			// {01  +1~63������}+{n���ֽڵ�����},{01000000}������
				repeat = (*pData) & 0x3f; // ����������еĳ���
				pData++;
				for (int i = 1; i <= repeat; i++) {
					if (linePixels <= frame.Width) {
						*BmpBuffer = pal[*pData];
						BmpBuffer++;
						linePixels++;
						pData++;
					}
					else {
						lineNotOver = false;
					}
				}
				break;
			case 2: // {10******} ��ʾ�ظ�n������
				// {10  +6bit Times}+{1Byte Index}, ��ʾ�ظ�n�����ء�
				// {10  +�ظ�1~63��}+{0~255����ɫ������},{10000000}������
				repeat = (*pData) & 0x3f; // ����ظ��Ĵ���
				pData++;
				color = pal[*pData];
				for (int i = 1; i <= repeat; i++) {
					if (linePixels <= frame.Width) {
						*BmpBuffer = color;
						BmpBuffer++;
						linePixels++;
					}
					else {
						lineNotOver = false;
					}
				}
				pData++;
				break;
			case 3: // {11******} ��ʾ����n�����أ�������������͸��ɫ����
				// {11  +6bit Times}, ��ʾ����n�����أ�������������͸��ɫ���档
				// {11  +����1~63������},{11000000}������
				repeat = (*pData) & 0x3f; // ����ظ�����
				if (repeat == 0) {
					if (linePixels <= frame.Width) { //���⴦��
						BmpBuffer--;
						linePixels--;
					}
					else {
						lineNotOver = false;
					}
				}
				else {
					for (int i = 1; i <= repeat; i++) {
						if (linePixels <= frame.Width) {
							BmpBuffer++;
							linePixels++;
						}
						else {
							lineNotOver = false;
						}
					}
				}
				pData++;
				break;
			default: // һ�㲻�����������
				printf("WAS ERROR\n");
				break;
			}
		}
		if (*pData == 0 || !lineNotOver)
		{
			uint32_t repeat = frame.Width - linePixels;
			if (h > 0 && !linePixels) {//��������
				uint8_t* last = (uint8_t*)lp + FrameLineIndex[h - 1];
				if (*last != 0) {
					memcpy(BmpBuffer, BmpBuffer - frame.Width, frame.Width * 4);
					BmpBuffer += frame.Width;
				}
			}
			else if (repeat > 0) {
				BmpBuffer += repeat;
			}
		}
	}
	delete FrameLineIndex;
	return BP;
}

PyXY2::_PIXEL PyXY2::RGB565to888(unsigned short color, unsigned char Alpha) {
	_PIXEL pixel;

	unsigned int r = (color >> 11) & 0x1f;
	unsigned int g = (color >> 5) & 0x3f;
	unsigned int b = (color) & 0x1f;


	pixel.R = (r << 3) | (r >> 2);
	pixel.G = (g << 2) | (g >> 4);
	pixel.B = (b << 3) | (b >> 2);
	pixel.A = Alpha;

	return pixel;
}
