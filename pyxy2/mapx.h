#pragma once
#include <fstream>
#include <cstdint>
#include <vector>
#include <set>
#include <unordered_map>
#include "ujpeg.h"

struct MaskKeyHash
{
	template<class T1, class T2>
	std::size_t operator() (const std::pair<T1, T2>& p) const
	{
		auto h1 = std::hash<T1>{}(p.first);
		auto h2 = std::hash<T2>{}(p.second);
		return (h1 * 1000) + h2;
	}
};

class MapX {
public:
	struct EssenMaskInfo {
		int	StartX;
		int	StartY;
		uint32_t	Width;
		uint32_t	Height;
	};

	struct BasicMaskInfo : EssenMaskInfo {
		uint32_t	Size;
	};

	struct MaskInfo : BasicMaskInfo {
		int id;
		std::vector<uint8_t> RGBA;
		std::set<int> OccupyBlocks;
		uint32_t MaskOffset;
		uint32_t occupyRowStart;
		uint32_t occupyRowEnd;
		uint32_t occupyColStart;
		uint32_t occupyColEnd;
		bool bHasLoad = false;
		bool bLoading = false;
	};

	struct MapBlock {
		std::vector<uint8_t> JPEGRGB24;
		uint32_t Size;
		uint32_t Index;
		std::vector<uint8_t>  Cell;
		bool bHasLoad = false;
		bool bLoading = false;
		uint32_t JpegSize;
		uint32_t JpegOffset;
		std::set<int> OwnMasks;
	};

	MapX(std::string filename, int coordinate);

	// JPEG

	MapBlock* GetBlockInfo(int index) { return &m_Blocks[index]; };

	std::set<int> GetBlockMasks(int index) { return m_Blocks[index].OwnMasks; };

	bool ReadJPEG(int index);

	bool ReadJPEG(int row, int col) { return ReadJPEG(row * m_ColCount + col); };

	bool HasJPEGLoaded(int index) { return m_Blocks[index].bHasLoad; };

	uint8_t* GetJPEGRGB(int index) { return m_Blocks[index].JPEGRGB24.data(); };

	void EraseJPEGRGB(int index) { std::vector<uint8_t> v; m_Blocks[index].JPEGRGB24.swap(v); m_Blocks[index].bHasLoad = false; };

	// Mask

	MaskInfo* GetMaskInfo(int maskIndex) { return &m_Masks[maskIndex]; };

	void ReadMask(int index);

	void ReadMaskOrigin(int index);

	bool HasMaskLoaded(int index) { return m_Masks[index].bHasLoad; };

	uint8_t* GetMaskRGBA(int index) { return m_Masks[index].RGBA.data(); };

	void EraseMaskRGB(int index) { std::vector<uint8_t> v; m_Masks[index].RGBA.swap(v); m_Masks[index].bHasLoad = false; };

	// Cell

	uint32_t* GetCell() { return m_Cell.data(); };

	// Map

	int GetHeight() { return m_Height; }
	int GetWidth() { return m_Width; }
	int GetColCount() { return m_ColCount; }
	int GetRowCount() { return m_RowCount; }
	int GetCellColCount() { return m_CellColCount; }
	int GetCellRowCount() { return m_CellRowCount; }
	int GetMaskCount() { return m_Masks.size(); }

private:
	struct MapHeader {
		uint32_t		Flag;
		uint32_t		Width;
		uint32_t		Height;
	};

	struct BlockHeader {
		uint32_t		Flag;
		uint32_t		Size;
	};

	struct MaskHeader {
		uint32_t	Flag;
		int	Size;
	};

	struct JPEGHeader {
		uint32_t Flag;
		int	Size;
	};

	struct RGBA {
		uint8_t R;
		uint8_t G;
		uint8_t B;
		uint8_t A;
	};

	int m_ScanDirection;  // 扫描方向   0：从上至下   1：从下至上

	std::string m_FileName;  // 文件名

	std::uint64_t m_FileSize;  // 文件大小

	std::vector<uint8_t> m_FileData;  // 缓存数据，以释放文件句柄

	MapHeader m_Header;  // MapHeader

	int m_MapType;  // 新旧地图标志 1：旧地图  2：新地图

	int m_Width;  // 像素宽度

	int m_Height;  // 像素高度

	int m_BlockWidth;  // Map地图块像素宽度320

	int m_BlockHeight;  // Map地图块像素高度240

	uint32_t m_RowCount;  // 地图块行数目

	uint32_t m_ColCount;  // 地图块列数目

	int m_CellRowCount;  // 

	int m_CellColCount;  //

	std::vector<uint32_t> m_Cell;

	uint32_t m_BlockCount;  // 地图块数量

	std::vector<MapBlock> m_Blocks;  // 地图块

	std::vector<uint32_t> m_BlockOffsets;  // 地图索引缓存

	uint32_t m_MapSize;

	JPEGHeader m_JPEGHeaderInfo;

	std::vector<uint8_t> m_JPEGHeader;  // 旧地图JPEG Header

	MaskHeader m_MaskHeader;

	std::vector<MaskInfo> m_Masks;  // 索引

	std::vector<uint32_t> m_MaskOffsets;

	uint32_t m_MaskCount;

	void DecodeNewMapMasks();

	void DecodeOldMapMask(uint32_t offset, int blockIndex, int size);

	void DecodeMapBlocks();

	std::unordered_map<std::pair<unsigned int, unsigned int>, uint32_t, MaskKeyHash> m_NoRepeatMasks;

	void ReadCell(uint32_t offset, uint32_t size, uint32_t index);

	void ReadBrig(uint32_t offset, uint32_t size, uint32_t index);

	uJPEG m_ujpeg;

	void ByteSwap(uint16_t& value);

	void MapHandler(uint8_t* Buffer, uint32_t inSize, uint8_t* outBuffer, uint32_t* outSize);

	size_t DecompressMask(void* in, void* out);

	RGBA ReadPixel(int x, int y);
};