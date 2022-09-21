#include "pch.h"
#include "mapx.h"
#include <iostream>
#include <string>
#include <memory>
#include<windows.h>
using std::ios;

#define MEM_READ_WITH_OFF(off,dst,src,len) if(off+len<=src.size()){  memcpy((uint8_t*)dst,(uint8_t*)(src.data()+off),len);off+=len;   }
#define MEM_COPY_WITH_OFF(off,dst,src,len) {  memcpy(dst,src+off,len);off+=len;   }


MapX::MapX(std::string filename, int direction) :m_FileName(filename), m_ScanDirection(direction) {
	std::fstream fs(m_FileName, ios::in | ios::binary);
	if (!fs) {
		std::cerr << "Map file open error!" << m_FileName << std::endl;
		return;
	}
	std::clog << "InitMAP:" << m_FileName.c_str() << std::endl;

	auto fpos = fs.tellg();
	fs.seekg(0, std::ios::end);
	m_FileSize = fs.tellg() - fpos;

	m_FileData.resize(m_FileSize);
	fs.seekg(0, std::ios::beg);
	fs.read((char*)m_FileData.data(), m_FileSize);
	fs.close();

	uint32_t fileOffset = 0;
	MEM_READ_WITH_OFF(fileOffset, &m_Header, m_FileData, sizeof(MapHeader));
	if (m_Header.Flag == 0x4D312E30) {  // M1.0
		m_MapType = 2;
	}
	else if (m_Header.Flag == 0x4D415058) {  // MAPX
		m_MapType = 1;
	}
	else {
		std::cerr << "Map file format error!" << std::endl;
		return;
	}

	m_Width = m_Header.Width;
	m_Height = m_Header.Height;

	m_BlockWidth = 320;
	m_BlockHeight = 240;

	m_ColCount = (uint32_t)std::ceil(m_Header.Width * 1.0f / m_BlockWidth);
	m_RowCount = (uint32_t)std::ceil(m_Header.Height * 1.0f / m_BlockHeight);

	m_CellRowCount = m_Height / 20;
	m_CellRowCount = m_CellRowCount % 12 == 0 ? m_CellRowCount : m_CellRowCount + 12 - m_CellRowCount % 12;
	m_CellColCount = m_Width / 20;
	m_CellColCount = m_CellColCount % 16 == 0 ? m_CellColCount : m_CellColCount + 16 - m_CellColCount % 16;
	m_Cell.resize(m_CellRowCount * m_CellColCount);

	m_BlockCount = m_RowCount * m_ColCount;
	m_Blocks.resize(m_BlockCount);
	m_BlockOffsets.resize(m_BlockCount, 0);
	MEM_READ_WITH_OFF(fileOffset, m_BlockOffsets.data(), m_FileData, m_BlockCount * 4);

	if (m_MapType == 1) {  // 旧地图，读取JPEG Header
		MEM_READ_WITH_OFF(fileOffset, &m_MapSize, m_FileData, 4);
		MEM_READ_WITH_OFF(fileOffset, &m_JPEGHeaderInfo, m_FileData, sizeof(JPEGHeader));
		if (m_JPEGHeaderInfo.Flag == 0x4A504748) {
			m_JPEGHeader.resize(m_JPEGHeaderInfo.Size);
			MEM_READ_WITH_OFF(fileOffset, m_JPEGHeader.data(), m_FileData, m_JPEGHeaderInfo.Size);
		}
		//  m_JPEGDecoder.LoadHeader(m_JPEGHeader.data());  // 旧地图读取JPEG头
	}
	else if (m_MapType == 2) {  // 新地图，读取Mask索引
		MEM_READ_WITH_OFF(fileOffset, &m_MaskHeader, m_FileData, sizeof(MaskHeader));
		m_MaskCount = m_MaskHeader.Size;
		m_Masks.resize(m_MaskCount);
		m_MaskOffsets.resize(m_MaskCount, 0);
		MEM_READ_WITH_OFF(fileOffset, m_MaskOffsets.data(), m_FileData, m_MaskCount * 4);

		DecodeNewMapMasks();
	}

	DecodeMapBlocks();  // 读取JPEG基本信息和旧地图Mask索引

	std::clog << "MAP init success!" << std::endl;
}

void MapX::DecodeNewMapMasks() {
	for (size_t index = 0; index < m_MaskCount; index++)
	{
		uint32_t offset = m_MaskOffsets[index];

		BasicMaskInfo basicMaskInfo{ };
		MEM_READ_WITH_OFF(offset, &basicMaskInfo, m_FileData, sizeof(BasicMaskInfo));

		MaskInfo& maskInfo = m_Masks[index];
		maskInfo.id = index;
		maskInfo.StartX = basicMaskInfo.StartX;
		maskInfo.StartY = basicMaskInfo.StartY;
		maskInfo.Width = basicMaskInfo.Width;
		maskInfo.Height = basicMaskInfo.Height;
		maskInfo.Size = basicMaskInfo.Size;
		maskInfo.MaskOffset = offset;

		maskInfo.occupyRowStart = maskInfo.StartY / m_BlockHeight;
		maskInfo.occupyRowEnd = (maskInfo.StartY + maskInfo.Height) / m_BlockHeight;

		maskInfo.occupyColStart = maskInfo.StartX / m_BlockWidth;
		maskInfo.occupyColEnd = (maskInfo.StartX + maskInfo.Width) / m_BlockWidth;

		for (int i = maskInfo.occupyRowStart; i <= maskInfo.occupyRowEnd; i++)
			for (int j = maskInfo.occupyColStart; j <= maskInfo.occupyColEnd; j++)
			{
				int unit = i * m_ColCount + j;
				if (unit >= 0 && unit < m_Blocks.size())
				{
					maskInfo.OccupyBlocks.insert(unit);
					m_Blocks[unit].OwnMasks.insert((int)index);
				}
			}
	}
}

void MapX::DecodeOldMapMask(uint32_t offset, int blockIndex, int size) {
	EssenMaskInfo essenMaskInfo{ };
	MEM_READ_WITH_OFF(offset, &essenMaskInfo, m_FileData, sizeof(EssenMaskInfo));

	int row = blockIndex / m_ColCount;
	int col = blockIndex % m_ColCount;
	essenMaskInfo.StartX = (col * 320) + essenMaskInfo.StartX;
	essenMaskInfo.StartY = (row * 240) + essenMaskInfo.StartY;

	auto key = std::make_pair(essenMaskInfo.StartX, essenMaskInfo.StartY);
	if (m_NoRepeatMasks.count(key) == 0) {  // 未找到，即为新Mask
		int id = m_NoRepeatMasks.size();  // id 从0开始
		m_NoRepeatMasks[key] = id;  // 插入到m_NoRepeatMasks

		MaskInfo maskInfo;
		maskInfo.id = id;
		maskInfo.StartX = essenMaskInfo.StartX;
		maskInfo.StartY = essenMaskInfo.StartY;
		maskInfo.Width = essenMaskInfo.Width;
		maskInfo.Height = essenMaskInfo.Height;
		maskInfo.Size = size - sizeof(essenMaskInfo);
		maskInfo.MaskOffset = offset;
		maskInfo.OccupyBlocks.insert(blockIndex);
		m_Blocks[blockIndex].OwnMasks.insert((int)id);

		maskInfo.occupyRowStart = maskInfo.StartY / m_BlockHeight;
		maskInfo.occupyRowEnd = (maskInfo.StartY + maskInfo.Height) / m_BlockHeight;

		maskInfo.occupyColStart = maskInfo.StartX / m_BlockWidth;
		maskInfo.occupyColEnd = (maskInfo.StartX + maskInfo.Width) / m_BlockWidth;

		m_Masks.push_back(maskInfo);
	}
	else {  // 找到，更新信息
		int id = m_NoRepeatMasks[key];
		m_Blocks[blockIndex].OwnMasks.insert(id);
		m_Masks[id].OccupyBlocks.insert(blockIndex);
	}
}

void MapX::DecodeMapBlocks() {
	for (size_t i = 0; i < m_Blocks.size(); i++) {
		uint32_t offset = m_BlockOffsets[i];
		uint32_t eatNum;
		MEM_READ_WITH_OFF(offset, &eatNum, m_FileData, sizeof(uint32_t));
		if (m_MapType == 2)
			offset += eatNum * 4;
		bool loop = true;
		while (loop) {
			BlockHeader blockHeader{ 0 };
			MEM_READ_WITH_OFF(offset, &blockHeader, m_FileData, sizeof(BlockHeader));
			switch (blockHeader.Flag) {
			case 0x4A504732:	// JPG2
			case 0x4A504547: {  // JPEG
				m_Blocks[i].JpegOffset = offset;
				m_Blocks[i].JpegSize = blockHeader.Size;
				offset += blockHeader.Size;
				break;
			}
			case 0x4D415332:	// MAS2
			case 0x4D41534B: {	// MASK
				DecodeOldMapMask(offset, i, blockHeader.Size);
				offset += blockHeader.Size;
				break;
			}
			case 0x43454C4C:  // CELL
				ReadCell(offset, (uint32_t)blockHeader.Size, (uint32_t)i);
				offset += blockHeader.Size;
				break;
			case 0x42524947:  // BRIG
				offset += blockHeader.Size;
				break;
			case 0x424c4f4b:  // BLOK
				offset += blockHeader.Size;
				break;
			default:
				loop = false;
				break;
			}
		}
	}
}

void MapX::ReadCell(uint32_t offset, uint32_t size, uint32_t index) {
	m_Blocks[index].Cell.resize(size, 0);
	MEM_READ_WITH_OFF(offset, m_Blocks[index].Cell.data(), m_FileData, size);
	int cellRow = (index / m_ColCount) * 12;
	int cellCol = (index % m_ColCount) * 16;
	int count = 0;
	for (int i = 0; i < m_Blocks[index].Cell.size(); i++) {
		if (cellRow * m_CellColCount + cellCol + count > m_Cell.size()) {
			std::cout << "?" << std::endl;
		}
		if (i > m_Blocks[index].Cell.size()) {
			std::cout << "!" << std::endl;
		}
		m_Cell[cellRow * m_CellColCount + cellCol + count] = m_Blocks[index].Cell[i];
		count++;
		if (count >= 16) {
			count = 0;
			cellRow++;
		}
	}
}

void  MapX::ReadBrig(uint32_t offset, uint32_t size, uint32_t index) {
	offset += size;
}

bool MapX::ReadJPEG(int index) {
	if (m_Blocks[index].bHasLoad || m_Blocks[index].bLoading) {
		return m_Blocks[index].bHasLoad;
	}
	m_Blocks[index].bLoading = true;

	int size = m_Blocks[index].JpegSize;
	std::vector<uint8_t> jpegData(size, 0);
	MEM_READ_WITH_OFF(m_Blocks[index].JpegOffset, jpegData.data(), m_FileData, size);

	uint32_t tmpSize = 0;
	if (m_MapType == 1) {
		jpegData.insert(jpegData.begin(), m_JPEGHeader.begin(), m_JPEGHeader.end());
		jpegData.push_back(0xff);
		jpegData.push_back(0xd9);
		bool result = m_ujpeg.decode(jpegData.data(), jpegData.size(), true);
		if (!result)
			return 0;
		if (!m_ujpeg.isValid())
			return 0;
		m_Blocks[index].JPEGRGB24.resize(230400);
		m_ujpeg.getImage(m_Blocks[index].JPEGRGB24.data());
	}
	else {
		m_Blocks[index].JPEGRGB24.resize(size * 2, 0);
		MapHandler(jpegData.data(), size, m_Blocks[index].JPEGRGB24.data(), &tmpSize);
		bool result = m_ujpeg.decode(m_Blocks[index].JPEGRGB24.data(), tmpSize, false);
		if (!result)
			return 0;
		if (!m_ujpeg.isValid())
			return 0;
		m_Blocks[index].JPEGRGB24.resize(230400);
		m_ujpeg.getImage(m_Blocks[index].JPEGRGB24.data());
	}

	m_Blocks[index].bHasLoad = true;
	m_Blocks[index].bLoading = false;

	return m_Blocks[index].bHasLoad;
}

void MapX::ByteSwap(uint16_t& value) {
	uint16_t tempvalue = value >> 8;
	value = (value << 8) | tempvalue;
}

void MapX::MapHandler(uint8_t* Buffer, uint32_t inSize, uint8_t* outBuffer, uint32_t* outSize) {
	// JPEG数据处理原理
	// 1、复制D8到D9的数据到缓冲区中
	// 2、删除第3、4个字节 FFA0
	// 3、修改FFDA的长度00 09 为 00 0C
	// 4、在FFDA数据的最后添加00 3F 00
	// 5、替换FFDA到FF D9之间的FF数据为FF 00

	uint32_t TempNum = 0;						// 临时变量，表示已读取的长度
	uint16_t TempTimes = 0;					// 临时变量，表示循环的次数
	uint32_t Temp = 0;
	bool break_while = false;

	// 当已读取数据的长度小于总长度时继续
	while (!break_while && TempNum < inSize && *Buffer++ == 0xFF)
	{
		*outBuffer++ = 0xFF;
		TempNum++;
		switch (*Buffer)
		{
		case 0xD8:
			*outBuffer++ = 0xD8;
			Buffer++;
			TempNum++;
			break;
		case 0xA0:
			Buffer++;
			outBuffer--;
			TempNum++;
			break;
		case 0xC0:
			*outBuffer++ = 0xC0;
			Buffer++;
			TempNum++;

			memcpy(&TempTimes, Buffer, sizeof(uint16_t)); // 读取长度
			ByteSwap(TempTimes); // 将长度转换为Intel顺序


			for (int i = 0; i < TempTimes; i++)
			{
				*outBuffer++ = *Buffer++;
				TempNum++;
			}

			break;
		case 0xC4:
			*outBuffer++ = 0xC4;
			Buffer++;
			TempNum++;
			memcpy(&TempTimes, Buffer, sizeof(uint16_t)); // 读取长度
			ByteSwap(TempTimes); // 将长度转换为Intel顺序

			for (int i = 0; i < TempTimes; i++)
			{
				*outBuffer++ = *Buffer++;
				TempNum++;
			}
			break;
		case 0xDB:
			*outBuffer++ = 0xDB;
			Buffer++;
			TempNum++;

			memcpy(&TempTimes, Buffer, sizeof(uint16_t)); // 读取长度
			ByteSwap(TempTimes); // 将长度转换为Intel顺序

			for (int i = 0; i < TempTimes; i++)
			{
				*outBuffer++ = *Buffer++;
				TempNum++;
			}
			break;
		case 0xDA:
			*outBuffer++ = 0xDA;
			*outBuffer++ = 0x00;
			*outBuffer++ = 0x0C;
			Buffer++;
			TempNum++;

			memcpy(&TempTimes, Buffer, sizeof(uint16_t)); // 读取长度
			ByteSwap(TempTimes); // 将长度转换为Intel顺序
			Buffer++;
			TempNum++;
			Buffer++;

			for (int i = 2; i < TempTimes; i++)
			{
				*outBuffer++ = *Buffer++;
				TempNum++;
			}
			*outBuffer++ = 0x00;
			*outBuffer++ = 0x3F;
			*outBuffer++ = 0x00;
			Temp += 1; // 这里应该是+3的，因为前面的0xFFA0没有-2，所以这里只+1。

					   // 循环处理0xFFDA到0xFFD9之间所有的0xFF替换为0xFF00
			for (; TempNum < inSize - 2;)
			{
				if (*Buffer == 0xFF)
				{
					*outBuffer++ = 0xFF;
					*outBuffer++ = 0x00;
					Buffer++;
					TempNum++;
					Temp++;
				}
				else
				{
					*outBuffer++ = *Buffer++;
					TempNum++;
				}
			}
			// 直接在这里写上了0xFFD9结束Jpeg图片.
			Temp--; // 这里多了一个字节，所以减去。
			outBuffer--;
			*outBuffer-- = 0xD9;
			break;
		case 0xD9:
			// 算法问题，这里不会被执行，但结果一样。
			*outBuffer++ = 0xD9;
			TempNum++;
			break;
		case 0xE0:
			break_while = true;  // 如果碰到E0,则说明后面的数据不需要修复
			while (TempNum < inSize) {
				*outBuffer++ = *Buffer++;
				TempNum++;
			}
			break;
		default:
			break;
		}
	}
	Temp += inSize;
	*outSize = Temp;
}

size_t MapX::DecompressMask(void* in, void* out)
{
	uint8_t* op;
	uint8_t* ip;
	unsigned t;
	uint8_t* m_pos;

	op = (uint8_t*)out;
	ip = (uint8_t*)in;

	if (*ip > 17) {
		t = *ip++ - 17;
		if (t < 4)
			goto match_next;
		do *op++ = *ip++; while (--t > 0);
		goto first_literal_run;
	}

	while (1) {
		t = *ip++;
		if (t >= 16) goto match;
		if (t == 0) {
			while (*ip == 0) {
				t += 255;
				ip++;
			}
			t += 15 + *ip++;
		}

		*(unsigned*)op = *(unsigned*)ip;
		op += 4; ip += 4;
		if (--t > 0)
		{
			if (t >= 4)
			{
				do {
					*(unsigned*)op = *(unsigned*)ip;
					op += 4; ip += 4; t -= 4;
				} while (t >= 4);
				if (t > 0) do *op++ = *ip++; while (--t > 0);
			}
			else do *op++ = *ip++; while (--t > 0);
		}

	first_literal_run:

		t = *ip++;
		if (t >= 16)
			goto match;

		m_pos = op - 0x0801;
		m_pos -= t >> 2;
		m_pos -= *ip++ << 2;

		*op++ = *m_pos++; *op++ = *m_pos++; *op++ = *m_pos;

		goto match_done;

		while (1)
		{
		match:
			if (t >= 64)
			{

				m_pos = op - 1;
				m_pos -= (t >> 2) & 7;
				m_pos -= *ip++ << 3;
				t = (t >> 5) - 1;

				goto copy_match;

			}
			else if (t >= 32)
			{
				t &= 31;
				if (t == 0) {
					while (*ip == 0) {
						t += 255;
						ip++;
					}
					t += 31 + *ip++;
				}

				m_pos = op - 1;
				m_pos -= (*(unsigned short*)ip) >> 2;
				ip += 2;
			}
			else if (t >= 16) {
				m_pos = op;
				m_pos -= (t & 8) << 11;
				t &= 7;
				if (t == 0) {
					while (*ip == 0) {
						t += 255;
						ip++;
					}
					t += 7 + *ip++;
				}
				m_pos -= (*(unsigned short*)ip) >> 2;
				ip += 2;
				if (m_pos == op)
					goto eof_found;
				m_pos -= 0x4000;
			}
			else {
				m_pos = op - 1;
				m_pos -= t >> 2;
				m_pos -= *ip++ << 2;
				*op++ = *m_pos++; *op++ = *m_pos;
				goto match_done;
			}

			if (t >= 6 && (op - m_pos) >= 4) {
				*(unsigned*)op = *(unsigned*)m_pos;
				op += 4; m_pos += 4; t -= 2;
				do {
					*(unsigned*)op = *(unsigned*)m_pos;
					op += 4; m_pos += 4; t -= 4;
				} while (t >= 4);
				if (t > 0) do *op++ = *m_pos++; while (--t > 0);
			}
			else {
			copy_match:
				*op++ = *m_pos++; *op++ = *m_pos++;
				do *op++ = *m_pos++; while (--t > 0);
			}

		match_done:

			t = ip[-2] & 3;
			if (t == 0)	break;

		match_next:
			do *op++ = *ip++; while (--t > 0);
			t = *ip++;
		}
	}

eof_found:
	return (op - (uint8_t*)out);
}

void MapX::ReadMaskOrigin(int index) {
	if (m_Masks[index].bHasLoad || m_Masks[index].bLoading) {
		return;
	}
	m_Masks[index].bLoading = true;

	int fileOffset = m_Masks[index].MaskOffset;
	std::vector<uint8_t> pData(m_Masks[index].Size, 0);
	MEM_READ_WITH_OFF(fileOffset, pData.data(), m_FileData, m_Masks[index].Size);

	int align_width = (m_Masks[index].Width + 3) / 4;	// align 4 bytes
	int size = align_width * m_Masks[index].Height;
	std::vector<uint8_t> pMaskDataDec(size, 0);

	DecompressMask(pData.data(), pMaskDataDec.data());

	// 至此所需图块全部加载完毕，现在读取像素
	m_Masks[index].RGBA.resize(m_Masks[index].Width * m_Masks[index].Height * 4, 255);

	int maskIndex = 0;
	for (int i = 0; i < m_Masks[index].Height; i++) {
		int counter4 = 0;
		for (int j = 0; j < m_Masks[index].Width; j++) {
			int pos = i * m_Masks[index].Width + j;
			uint8_t byte = pMaskDataDec[maskIndex];

			uint8_t flag = (byte >> counter4 * 2) & 3;
			counter4++;
			if (counter4 >= 4) {
				counter4 = 0;
				maskIndex++;
			}

			if (flag > 0) {
				int cur = pos;
				if (m_ScanDirection == 1) {
					cur = (m_Masks[index].Height - 1 - i) * m_Masks[index].Width + j;
				}

				m_Masks[index].RGBA[cur * 4 + 3] = flag == 3 ? 150 : 1;
			}
		}
		if (counter4 != 0) {
			counter4 = 0;
			maskIndex++;
		}
	}

	m_Masks[index].bHasLoad = true;
	m_Masks[index].bLoading = false;
}


void MapX::ReadMask(int index) {
	if (m_Masks[index].bHasLoad || m_Masks[index].bLoading) {
		return;
	}
	m_Masks[index].bLoading = true;

	int fileOffset = m_Masks[index].MaskOffset;
	std::vector<uint8_t> pData(m_Masks[index].Size, 0);
	MEM_READ_WITH_OFF(fileOffset, pData.data(), m_FileData, m_Masks[index].Size);

	int align_width = (m_Masks[index].Width + 3) / 4;	// align 4 bytes
	int size = align_width * m_Masks[index].Height;
	std::vector<uint8_t> pMaskDataDec(size, 0);

	DecompressMask(pData.data(), pMaskDataDec.data());

	for (int i = m_Masks[index].occupyRowStart; i <= m_Masks[index].occupyRowEnd; i++)
		for (int j = m_Masks[index].occupyColStart; j <= m_Masks[index].occupyColEnd; j++)
			while (!ReadJPEG(i, j))  // 读取所有涉及到的图块
				Sleep(100);  // 走到这里，即为Block -> bHasLoad 为false，bLoading为true，即有其他线程在读当前块。等待100ms

	// 至此所需图块全部加载完毕，现在读取像素
	m_Masks[index].RGBA.resize(m_Masks[index].Width * m_Masks[index].Height * 4, 0);  // 全部初始化为全透明

	int maskIndex = 0;
	for (int i = 0; i < m_Masks[index].Height; i++) {
		int counter4 = 0;
		for (int j = 0; j < m_Masks[index].Width; j++) {
			int pos = i * m_Masks[index].Width + j;
			uint8_t byte = pMaskDataDec[maskIndex];

			uint8_t flag = (byte >> counter4 * 2) & 3;
			counter4++;
			if (counter4 >= 4) {
				counter4 = 0;
				maskIndex++;
			}

			if (flag > 0) {
				int cur = pos;
				if (m_ScanDirection == 1) {
					cur = (m_Masks[index].Height - 1 - i) * m_Masks[index].Width + j;
				}
				RGBA rgba = ReadPixel(m_Masks[index].StartX + j, m_Masks[index].StartY + i);  // 读像素
				m_Masks[index].RGBA[cur * 4] = rgba.R;
				m_Masks[index].RGBA[cur * 4 + 1] = rgba.G;
				m_Masks[index].RGBA[cur * 4 + 2] = rgba.B;
				m_Masks[index].RGBA[cur * 4 + 3] = flag == 3 ? 150 : 1;
			}
		}
		if (counter4 != 0) {
			counter4 = 0;
			maskIndex++;
		}
	}

	m_Masks[index].bHasLoad = true;
	m_Masks[index].bLoading = false;
}

MapX::RGBA MapX::ReadPixel(int x, int y) {
	int row = y / m_BlockHeight;
	int col = x / m_BlockWidth;
	int index = row * m_ColCount + col;

	int rx = x % m_BlockWidth;
	int ry = y % m_BlockHeight;
	int pos = 0;
	if (m_ScanDirection == 0) {
		pos = (ry * m_BlockWidth + rx) * 3;
	}
	else {
		pos = ((m_BlockHeight - 1 - ry) * m_BlockWidth + rx) * 3;
	}

	RGBA rgba;

	rgba.R = m_Blocks[index].JPEGRGB24[pos];
	rgba.G = m_Blocks[index].JPEGRGB24[pos + 1];
	rgba.B = m_Blocks[index].JPEGRGB24[pos + 2];

	return rgba;
}
