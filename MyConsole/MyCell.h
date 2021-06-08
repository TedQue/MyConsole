#pragma once

/* 2015.6.12 - 实现思路描述

整个图像由一系列的单元格排列好组成,单元格可以是字符或者其它任何支持 MyCell 接口的类.
这个图像是一个虚拟图像,只以某种形式抽象的存在于内存中,仅仅显示窗口范围内的部分.

1. 所有单元格存储在一个一维数组中.
2. 使用一个行索引通过记录每行的起始位置把上述的数组解释成二维数组.
3. 这个二维单元格数组按行列排列最终组成了上述的虚拟图像,虚拟图像的概念是实现的基础.
4. 窗口显示的内容只是虚拟图像中的一部分或者全部,通过计算可以得出需要显示的单元格,显示时,先把需要显示的单元格绘制在一个内存BMP,然后再把这个BMP贴到窗口.
5. 行索引中,每行占用一项,记录该行 index 起始序号, width 行宽度,该行的虚拟图像中的Y轴 y 的位置,并在最后设置一个哨兵项.
6. 哨兵项 index = 所有单元格的个数, width = 0, y = 虚拟图像的高度. 哨兵项的存在使得总是可以用第 n + 1 行的索引项 和第 n 行的索引项相减得到第 n 行的单元格序号范围,行高度.
7. 行索引根据单元格的信息构建,更新时,尽量利用原有的信息. MyVirtualImage::updateRowIndex() 代码中的注释说明了如何尽可能利用原有结构维护行索引.
8. 每个单元格保存该单元格在虚拟图像中的行序号,称为反向索引.
9. 用户通过(行,列),像素坐标来访问虚拟图像; 内部使用一维数组的序号(即单元格序号)来访问虚拟图像.通过行索引和反向索引可以在3个坐标系内转换.

总结的时候,那么几句话就说完了,实际编码中有大量的细节问题需要处理,尤其是各种临界情况.

*/

/*
* 单元格接口
*/
class MyCell
{
private:
	int _rowIndex;

protected:
	MyCell() {};
	virtual ~MyCell() {};

public:
	// 反向行索引
	void setRowIndex(int idx) { _rowIndex = idx; }
	int getRowIndex() const {return _rowIndex; }

	// 重画时由框架调用(使用MyVirtualImage坐标系)
	virtual void draw(HDC hDc, const RECT* rc, bool selected) = 0;

	// 单元格从MyVirtualImage 中被删除时由框架调用
	virtual void remove() = 0;

	// 获取单元格的文本(str == NULL 时计算长度)
	virtual int getText(wchar_t* str, int len) const = 0;

	// 显示单元格后是否需要换行
	virtual int width() const = 0;
	virtual int height() const = 0;
	virtual bool wrap() const = 0;
};

// 字符单元格实现
class MyCellCharacterFactory;
class MyCellCharacter : public MyCell
{
protected:
	MyCellCharacterFactory* _ownerFactory;
	int _chIndex;		// 在 MyCellCharacterFactory 中的序号
	wchar_t _ch;
	int _width;
	int _height;
	COLORREF _fgClr;
	COLORREF _bkClr;	
	
	MyCellCharacter(const MyCellCharacter& oth);
	MyCellCharacter& operator = (const MyCellCharacter& oth);
public: 
	MyCellCharacter();
	virtual ~MyCellCharacter();

	void updateContext(MyCellCharacterFactory* owner, wchar_t ch, const POINT& sz, COLORREF fgClr, COLORREF bkClr);
	void updateSize(const POINT& sz);
	wchar_t getChar() const { return _ch; }
	int getCharIndex() const { return _chIndex; }
	void setCharIndex(int idx) { _chIndex = idx; }
	void setOwner(MyCellCharacterFactory* owner) { _ownerFactory = owner; }

	virtual void draw(HDC hDc, const RECT* rc, bool selected);
	virtual void remove();
	virtual int getText(wchar_t* str, int len) const;
	virtual int width() const;
	virtual int height() const;
	virtual bool wrap() const { return _ch == L'\n'; }
};

/*
* 字符单元格工厂 - 提高字符单元格的性能,避免频繁 new delete,统一管理字体等所有字符单元格能共享的资源
* 管理字体,字符相关的所有信息
*/
class MyCellCharacterFactory
{
private:
	typedef struct
	{
		MyCellCharacter* buf;	// 字符块的地址,长度为 CHARSBUF_BLOCK_LEN
		int ref;				// 该字符块的引用计数 ref = 0 空闲, ref = CHARSBUF_BLOCK_LEN 全部被占用
	}charsblock_t;
	charsblock_t* _blocks;		// 字符块缓冲
	int _blockSize;				// 每个字符块的大小
	int _blocksLen;				// 字符块缓冲长度
	int _curBlock;				// 指向当前字符块
	int _curIndex;				// 指向当前字符块的第一个空闲字符位置

	HDC _hdc;
	HFONT _hFont;
	int _charHeight;			// 行高
	int _charAveWidth;			// 字符的平均宽度参考值
	int _tabWidth;				// TAB 的宽度(像素)
	int getCharWidth(wchar_t ch) const;						// 获取一个字符的宽度
	int reserve(int len, MyCellCharacter** buf);		// 保留len个字符
	int findBlock(MyCellCharacter* ch);
	void swapBlock(int i, int j);			// 交换位置i 和 位置j 的字符单元
public:
	MyCellCharacterFactory(int blockSize);
	virtual ~MyCellCharacterFactory();

	void setFont(HDC hdc, HFONT font, int tabSize);
	int getCharAveWidth() const;
	int getCharHeight() const;

	/*
	* 为了提高效率,按"块操作"的方式创建字符单元.
	*/
	int alloc(const wchar_t* ch, int len, COLORREF fgClr, COLORREF bkClr, MyCellCharacter** chCells, int* cellLen);
	void recycle(MyCellCharacter** chs, int len);
};

/*
* 虚拟图像实现 MyVirtualImage
*/
// 行内的对齐方式
#define MYALIGN_TOP 0
#define MYALIGN_CENTER 1
#define MYALIGN_BOTTOM 2

// 通过坐标测算位置时返回的区域 hitTest()
#define MYHTC_LEFT		0x0001
#define MYHTC_RIGHT		0x0002
#define MYHTC_TOP		0x0010
#define MYHTC_BOTTOM	0x0020
#define MYHTC_ROWOVF	0x0100		// row overflow
#define MYHTC_COLOVF	0X0200		// column overflow

class MyVirtualImage
{
private:
	MyCell** _cells;			// 缓冲区(用一维数组模拟二维数组,这样便于修改行和列.
	int _cellBufLen;			// 缓冲长度
	int _curCellPos;			// 指向第一个可写的单元格位置(即当前的字符数)
	int _maxCellLen;
	
	// 行索引
	typedef struct
	{
		int index;				// 每一行的起始序号
		int width;				// 行宽度 - 一行中所有单元格的长度之和(含字间距)
		int y;					// 行坐标的 y 值
	}rowinfo_t;
	rowinfo_t* _rowInfo;		// 行索引
	int _sentryRowPos;			// 哨兵位置,在最后一行的后一行,保存一些参数,确保总是能通过获取有效行的下一行得到信息以便计算行高,行范围. = 行数
	int _rowIndexBufLen;		// 行索引缓冲的长度
	int _widestRowIndex;		// 最宽的行的序号
	bool _autoWrap;				// 是否自动换行

	// 窗口图像的内存缓冲
	HDC _hdc;
	HBITMAP _hbmp;
	POINT _windowSize;
	POINT _windowPos;
	HBRUSH _windowBkBrush;
	POINT _sel;					// 选中区域 [x, y)
	POINT _dirtyRange;			// 脏区域 [x, y) 当 y = -1 时表示更新从单元 x 的起始坐标到整个可视窗口的结束坐标.
	int _colSpacing;			// 字间距
	int _rowSpacing;			// 行间距
	int _alignMode;				// 对齐方式

	MyCellCharacterFactory _charFactory;	// 字符工厂

	int reserve(int len);
	
	int setRowInfo(int row, int pos, int width, int y, unsigned int mask);	// 设置行索引,会自动维护行索引用到的内存.
	int getRowInfo(int row, unsigned int mask) const;
	
	int drawc(int pos, const POINT& pt);
	int markDirtyRange(const POINT& rng);					// 标记脏区域 [x, y) 字符序号最终会转换成虚拟图像内的一个区域
	int getWidestRow(const POINT& rng) const;

	// 更新行索引,利用原有的行索引数据. len < 0 表示删除; len > 0 表示添加.
	int updateRowIndex(int pos, int len);
	int updateWindow();
	
	int redraw();						// 重画窗口(视口)内的内容
	int rebuildRowIndex();				// 重建行索引

	// 禁止复制
	MyVirtualImage(const MyVirtualImage&);
	MyVirtualImage& operator = (const MyVirtualImage&);
public:
	MyVirtualImage(int maxChars, const POINT& windowSz, bool autoWrap, HDC hdc);
	virtual ~MyVirtualImage();

	/*
	* ScreenBuffer 属性
	*/

	// 是否自动换行
	bool enableAutoWrap(bool autoWrap);

	// 获取行高, 行间距, 字符平均宽度,字间距
	int getRowSpacing() const;
	int getCharSpacing() const;

	/*
	* 添加修改删除
	*/
	// remove(0, -1) 删除全部
	int insert(int pos, MyCell** cells, int len);
	int remove(int pos, int len);
	MyCell* getCell(int pos);
	const MyCell* getCell(int pos) const;

	int insert(int pos, const wchar_t* ch, int len, const COLORREF& fg, const COLORREF& bk);
	int getText(int pos, int len, wchar_t* dest, int destLen) const;
	
	/*
	* 视图相关函数
	*/
	// 设置字体, tabSize 表示 \t 占用几个空格
	// OPAQUE / TRANSPARENT
	HFONT setFont(HFONT font, int rowSpacing, int colSpacing, int tabSize);
	int getCharAveWidth() const;
	int getCharHeight() const;
	int setBkMode(int m);
	HBRUSH setBkBrush(HBRUSH bkBrush);

	// 返回所有文本都显示时窗口即虚拟图像的大小(用户以此设置滚动条的信息)
	POINT getImageSize() const;	
	
	// 设置视口的大小(这是一个相对费时的操作,所以用户应该尽量减少调用此函数的次数,比如不要让窗口大小连续变化,而是分段变化)
	int setWindowSize(const POINT& sz);		
	int setWindowPos(const POINT& pos);
	int render(HDC hdc, const RECT& dest);
	
	// 重画指定单元格
	int update(int pos, int len);	
	
	// 在虚拟图像中做 序号,坐标和行列的相互转换
	// MyVirtualImage 中所有字符都在一个"虚拟图像"中排列好了. 窗口中显示的内容只是这个虚拟图像的一部分.
	// pos 的有效范围是 [0, size()]. 其中 size() 是下一个输出字符的位置.

	// 把坐标和行列(可能包含无效范围)测算为有效的序号[0, _curCellPos]
	int hitTest(const POINT& pt, unsigned int* htc = NULL) const;
	int hitTest(int row, int col, unsigned int* htc = NULL) const;

	POINT getPoint(int pos) const;
	int getRowCol(int pos, int* row, int* col) const;
	POINT getRowRange(int row) const;		// 获得某行的字符序号范围(所有的范围都是[f, t))
	int getRowHeight(int row) const;
	int rows() const;
	int size() const;

	// 选中, getSel(): x = pos, y = len;
	int setSel(int pos, int len);
	int getSel(int* pos, int* len) const;
};