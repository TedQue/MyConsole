#include "stdafx.h"
#include <assert.h>
#include <time.h>
#include "MyCell.h"

/*///////////////////////////////////////////////////////////////////////////////////
* 常数定义 
*////////////////////////////////////////////////////////////////////////////////////
#define ROWINDEX_INC 200			// 行缓冲不足时增长的个数
#define CHARS_INC 1024				// 字符缓冲不足时增长的个数
#define CELLS_INC 1024				// 单元格缓冲不足是增长的个数

#define BLOCK_SIZE 2048
#define BLOCK_INDEX_INC 64

// 设置行信息的屏蔽字
#define MASK_ROWNONE	0
#define MASK_ROWINDEX	0x01
#define MASK_ROWWIDTH	0x02
#define MASK_ROWY		0x04
#define MASK_ALL		0xFF

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX4(a,b,c,d) (MAX(MAX((a),(b)),MAX((c),(d))))
#define MIN4(a,b,c,d) (MIN(MIN((a),(b)),MIN((c),(d))))

/*///////////////////////////////////////////////////////////////////////////////////
* 全局函数
*////////////////////////////////////////////////////////////////////////////////////
COLORREF revers_color(COLORREF clr)
{
	DWORD mask = 0xffffff;
	clr = ~clr;
	clr &= mask;
	return clr;
}

void FillSolidRect(HDC hdc, const RECT* rc, COLORREF clr)
{
	HBRUSH hBrush = CreateSolidBrush(clr);
	FillRect(hdc, rc, hBrush);
	DeleteObject(hBrush);
}

bool is_in_range(int pos, const POINT& rng)
{
	return pos >= rng.x && pos < rng.y;
}

template <typename T>
void swap(T& v1, T& v2)
{
	T tmp(v1);
	v1 = v2;
	v2 = tmp;
}
/*///////////////////////////////////////////////////////////////////////////////////
* MyCellCharacter
*////////////////////////////////////////////////////////////////////////////////////

MyCellCharacter::MyCellCharacter()
{
}

MyCellCharacter::~MyCellCharacter()
{
}

void MyCellCharacter::updateContext(MyCellCharacterFactory* owner, wchar_t ch, const POINT& sz, COLORREF fgClr, COLORREF bkClr)
{
	_ownerFactory = owner;
	_ch = ch;
	_width = sz.x;
	_height = sz.y;
	_fgClr = fgClr;
	_bkClr = bkClr;
}

void MyCellCharacter::updateSize(const POINT& sz)
{
	_width = sz.x;
	_height = sz.y;
}

void MyCellCharacter::draw(HDC hDc, const RECT* rc, bool selected)
{
	if(selected)
	{
		// 在选区内,绘制一个背景色
		SetTextColor(hDc, revers_color(_fgClr));
		SetBkColor(hDc, revers_color(_bkClr));

		if(_ch == L'\n')
		{
			RECT rcLineBreak = *rc;
			rcLineBreak.right = rcLineBreak.left + _ownerFactory->getCharAveWidth();
			FillSolidRect(hDc, &rcLineBreak, revers_color(_bkClr));
		}
		else
		{
			FillSolidRect(hDc, rc, revers_color(_bkClr));
		}
	}
	else
	{
		// 在选区外
		SetTextColor(hDc, _fgClr);
		SetBkColor(hDc, _bkClr);
	}

	if(_width <= 0 || _height <= 0 || L'\t' == _ch)
	{
	}
	else
	{
		// 输出文本
		TextOut(hDc, rc->left, rc->top, &_ch, 1);

		// 文本下面输出一条虚线
		//SelectObject(hDc, GetStockObject(WHITE_PEN));
		//MoveToEx(hDc, rc->left, rc->bottom, NULL);
		//LineTo(hDc, rc->right, rc->bottom);
	}
}

void MyCellCharacter::remove()
{
	MyCellCharacter* chs[] = {this};
	_ownerFactory->recycle(chs, 1);
}

int MyCellCharacter::getText(wchar_t* str, int len) const
{
	if(str) str[0] = _ch;
	return 1;
}

int MyCellCharacter::width() const
{ 
	return _width;
}

int MyCellCharacter::height() const
{
	return _height;
}

/*///////////////////////////////////////////////////////////////////////////////////
* MyCellCharacterFactory
*////////////////////////////////////////////////////////////////////////////////////
MyCellCharacterFactory::MyCellCharacterFactory(int blockSize)
{
	_blockSize = blockSize;
	_blocks = NULL;
	_blocksLen = 0;
	_curBlock = 0;
	_curIndex = 0;
}

MyCellCharacterFactory::~MyCellCharacterFactory()
{
	if(_blocks)
	{
		for(int i = 0; i < _blocksLen; ++i)
		{
			if(_blocks[i].buf) delete []_blocks[i].buf;
		}
		delete []_blocks;
	}
}

void MyCellCharacterFactory::setFont(HDC hdc, HFONT font, int tabSize)
{
	// 记录参数
	_hdc = hdc;
	_hFont = font;

	// 获取字体相关的信息
	LOGFONT lf = {0};
	GetObject(font, sizeof(LOGFONT), &lf);

	_tabWidth = tabSize * getCharWidth(L' ');
	_charAveWidth = lf.lfWidth;
	if(_charAveWidth <= 0)
	{
		_charAveWidth = getCharWidth(L' ');
	}
	_charHeight = lf.lfHeight;
	if(_charHeight == 0)
	{
		// 默认行高 16 像素
		_charHeight = 16;
	}
	else if(_charHeight > 0)
	{
	}
	else
	{
		_charHeight *= -1;
	}

	// 更新所有MyCellCharacter 的字符宽度和高度
	for(int i = 0; i < _blocksLen; ++i)
	{
		if(_blocks[i].buf)
		{
			for(int j = 0; j < _blockSize; ++j)
			{
				if(_blocks[i].buf[j].getChar() != 0)
				{
					_blocks[i].buf[j].updateSize({getCharWidth(_blocks[i].buf[j].getChar()), _charHeight});
				}
			}
		}
	}
}

int MyCellCharacterFactory::getCharWidth(wchar_t ch) const
{
	int w = 0;
	if(ch == L'\r' || ch == L'\n')
	{
	}
	else if(ch == L'\t')
	{
		w = _tabWidth;
	}
	else
	{
		GetCharWidth32(_hdc, ch, ch, &w);
	}
	return w;
}

int MyCellCharacterFactory::getCharHeight() const
{
	return _charHeight;
}

int MyCellCharacterFactory::getCharAveWidth() const
{
	return _charAveWidth;
}

//CHARSBUF_BLOCK_LEN
int MyCellCharacterFactory::reserve(int len, MyCellCharacter** buf)
{
	int rlen = 0;
	if(_curBlock < _blocksLen)
	{
		assert(_curIndex < _blockSize);

		// 第一次访问字符块之前初始化之
		if(_blocks[_curBlock].buf == NULL)
		{
			assert(_curIndex == 0);
			assert(_blocks[_curBlock].ref == 0);
			_blocks[_curBlock].buf = new MyCellCharacter[_blockSize];
		}

		// 直接把当前字符块剩余的字符返回,长度可能小于 len
		rlen = _blockSize - _curIndex;
		if(rlen > len) rlen = len;

		*buf = _blocks[_curBlock].buf + _curIndex;
		_curIndex += rlen;
		_blocks[_curBlock].ref += rlen;

		assert(_blocks[_curBlock].ref <= _curIndex);

		// 当前字符块已经用完
		if(_curIndex >= _blockSize)
		{
			_curBlock++;
			_curIndex = 0;
		}
	}
	else
	{
		// 当前字符块已经用完,分配一个新的字符块
		// 1. 确保字符块索引可用
		charsblock_t* tmp = new charsblock_t[_blocksLen + BLOCK_INDEX_INC];
		memset(tmp, 0, sizeof(charsblock_t) * (_blocksLen + BLOCK_INDEX_INC));
		if(_blocks)
		{
			memcpy(tmp, _blocks, sizeof(charsblock_t) * _blocksLen);
			delete []_blocks;
		}
		_blocks = tmp;
		_blocksLen += BLOCK_INDEX_INC;

		// 2. 从新字符块中返回
		return reserve(len, buf);
	}
	return rlen;
}

int MyCellCharacterFactory::alloc(const wchar_t* ch, int len, COLORREF fgClr, COLORREF bkClr, MyCellCharacter** chCells, int* cellLen)
{
	// 总是允许分配更多的内存,最大内存限制由使用 MyCellCharacterFactory 的 MyVirtualImage 控制
	*cellLen = reserve(len, chCells);
	assert(*cellLen > 0);
	for(int i = 0; i < *cellLen; ++i)
	{
		(*chCells)[i].updateContext(this, ch[i], {getCharWidth(ch[i]), getCharHeight()}, fgClr, bkClr);
	}
	return *cellLen;
}

int MyCellCharacterFactory::findBlock(MyCellCharacter* ch)
{
	for(int i = 0; i <= _curBlock; ++i)
	{
		if(ch >= _blocks[i].buf && ch < _blocks[i].buf + _blockSize)
		{
			return i;
		}
	}
	assert(0);
	return -1;
}

void MyCellCharacterFactory::recycle(MyCellCharacter** chs, int len)
{
	// 找到字符对应的字符块
	int blockIdx = 0;
	charsblock_t* blockPtr = NULL;

	for(int i = 0; i < len; ++i)
	{
		if(blockPtr && (chs[i] >= _blocks[i].buf && chs[i] < _blocks[i].buf + _blockSize))
		{
		}
		else
		{
			blockIdx = findBlock(chs[i]);
			blockPtr = &_blocks[blockIdx];
		}

		// 把字符块的引用计数 - 1
		blockPtr->ref--;

		// 如果字符块空闲(即引用计数 = 0)则把该字符块移动到队尾,准备下次使用
		if(0 == blockPtr->ref)
		{
			assert(blockIdx <= _curBlock);

			if(blockIdx == _curBlock)
			{
				// 释放的字符块就是当前字符块,则把 _curIndex 归零就表示字符块又可用了.
				_curIndex = 0;
			}
			else if(blockIdx == _curBlock - 1)
			{
				// 释放的字符块是当前字符块的前一个,只要做一次交换
				swapBlock(blockIdx, _curBlock);
				_curBlock--;
			}
			else
			{
				// 其它情况需要做两次交换:先把空闲字符块交换到当前块的前一个位置,然后再把当前块和前一块交换
				swapBlock(blockIdx, _curBlock - 1);
				swapBlock(_curBlock - 1, _curBlock);
				_curBlock--;
			}

			// 如果空闲的字符块太多了,则删除一些
			// ..
			// ..
		}
	}
}

void MyCellCharacterFactory::swapBlock(int i, int j)
{
	assert(!(i == j || i < 0 || j < 0 || i >= _blocksLen || j >= _blocksLen));
	swap(_blocks[i], _blocks[j]);
}

/*///////////////////////////////////////////////////////////////////////////////////
* MyVirtualImage
*////////////////////////////////////////////////////////////////////////////////////
MyVirtualImage::MyVirtualImage(int maxChars, const POINT& windowSz, bool autoWrap, HDC hdc)
	:_charFactory(BLOCK_SIZE)
{
	// 默认的字体是和系统控制台窗口一样的等宽字体
	_hdc = CreateCompatibleDC(hdc);

	// 行列字体的初始状态-未设置
	_rowSpacing = 0;
	_colSpacing = 0;
	_alignMode = MYALIGN_TOP;
	_autoWrap = autoWrap;

	// 起始窗口的大小和位置
	_hbmp = NULL;
	_windowPos.x = 0;
	_windowPos.y = 0;
	_windowSize.x = 0;
	_windowSize.y = 0;
	_windowBkBrush = NULL;
	_sel.x = 0;
	_sel.y = 0;
	_dirtyRange.x = 0;		// 整个区域初始状态都是脏的
	_dirtyRange.y = -1;

	// 字符缓冲
	_cells = NULL;
	_curCellPos = 0;
	_cellBufLen = 0;
	_maxCellLen = maxChars;
	
	// 起始行缓冲,写的时候调用 setRowInfo 会自动维护行缓冲的内容
	_rowInfo = NULL;
	_rowIndexBufLen = 0;
	_widestRowIndex = 0;
	_sentryRowPos = 0;
	setRowInfo(0, 0, 0, 0, MASK_ROWWIDTH | MASK_ROWINDEX | MASK_ROWY);
	
	// 设置默认格式 SYSTEM_FIXED_FONT / DEFAULT_GUI_FONT
	setFont((HFONT)GetStockObject(SYSTEM_FIXED_FONT), 0, 0, 4);
	setBkBrush((HBRUSH)GetStockObject(BLACK_BRUSH));
	//setBkMode(OPAQUE);  // 默认模式就是 OPAQUE
	//setSelColour(RGB(173,214,255));

	// 分配字符缓冲
	setWindowSize(windowSz);
}

MyVirtualImage::~MyVirtualImage()
{
	if(_hbmp) DeleteObject(_hbmp);
	if(_hdc) DeleteDC(_hdc);
	if(_cells) delete []_cells;
	if(_rowInfo) delete []_rowInfo;
}

HFONT MyVirtualImage::setFont(HFONT font, int rowSpacing, int colSpacing, int tabSize)
{
	// 1. 记录参数
	_colSpacing = colSpacing;
	_rowSpacing = rowSpacing;

	// 2. 选中字体,更新所有字符的宽度
	HFONT oldFont = (HFONT)SelectObject(_hdc, font);
	_charFactory.setFont(_hdc, font, tabSize);
	
	// 3. 根据新的参数重建行索引之后重画
	rebuildRowIndex();
	redraw();

	return oldFont;
}

int MyVirtualImage::getCharHeight() const
{
	return _charFactory.getCharHeight();
}

int MyVirtualImage::getCharAveWidth() const
{
	return _charFactory.getCharAveWidth();
}

int MyVirtualImage::getRowSpacing() const
{
	return _rowSpacing;
}

int MyVirtualImage::getCharSpacing() const
{
	return _colSpacing;
}

int MyVirtualImage::setBkMode(int m)
{
	int oldVal = SetBkMode(_hdc, m);
	redraw();
	return oldVal;
}

HBRUSH MyVirtualImage::setBkBrush(HBRUSH bkBrush)
{
	HBRUSH oldBrush = _windowBkBrush;
	_windowBkBrush = bkBrush;

	redraw();
	return oldBrush;
}

int MyVirtualImage::rows() const
{
	return _sentryRowPos;
}

int MyVirtualImage::size() const
{
	return _curCellPos;
}

// 获取行范围: 由于哨兵的存在,总是可以通过获取下一行的起始位置来计算含范围.
POINT MyVirtualImage::getRowRange(int row) const
{
	assert(row >= 0 && row < _sentryRowPos);
	return {getRowInfo(row, MASK_ROWINDEX), getRowInfo(row + 1, MASK_ROWINDEX)};
}

// 字符序号 -> 行列序号.由于单元格包含了一个反向索引所以可以直接得到行序号,否则需要用二分查找算法扫描行索引直到找到对应序号的行.
int MyVirtualImage::getRowCol(int pos, int* row, int* col) const
{
	assert(pos >= 0 && pos <= _curCellPos);
	if(pos == _curCellPos)
	{
		if(_sentryRowPos > 0)
		{
			*row = _sentryRowPos - 1;
			POINT rng = getRowRange(*row);
			*col = rng.y - rng.x;
		}
		else
		{
			*row = 0;
			*col = 0;
		}

		return 0;
	}

	int rowIndex = _cells[pos]->getRowIndex();
	if(row) *row = rowIndex;
	if(col)
	{
		POINT rng = getRowRange(rowIndex);
		assert(rng.x <= pos && rng.y > pos);
		*col = pos - rng.x;
	}
	return 0;
}

// 字符序号 -> 虚拟图像里的坐标(leftTop)
// pos 的有效范围是 [0, _curCellPos] 当 pos = _curCellPos 时返回最后一个字符后输出的位置,这是一个特殊情况的用途
POINT MyVirtualImage::getPoint(int pos) const
{
	assert(pos >= 0 && pos <= _curCellPos);
	POINT pt = {0, 0};
	if(pos >= _curCellPos)
	{
		// 返回最后一个字符的下一个字符的输出位置
		if(_curCellPos > 0)
		{
			if(_cells[_curCellPos - 1]->wrap())
			{
				pt.x = 0;
				pt.y = getRowInfo(_sentryRowPos - 1, MASK_ROWY);
			}
			else
			{
				pt.x = getRowInfo(_sentryRowPos - 1, MASK_ROWWIDTH) + _colSpacing;
				pt.y = getRowInfo(_sentryRowPos - 1, MASK_ROWY);
			}
		}
		else
		{
			pt.x = 0;
			pt.y = 0;
		}
	}
	else
	{
		// 返回对应字符的位置
		int row;
		getRowCol(pos, &row, NULL);
		pt.x = 0;
		POINT rng = getRowRange(row);
		for(int i = rng.x; i < pos; ++i)
		{
			pt.x += _cells[i]->width() + _colSpacing;
		}
		pt.y = getRowInfo(row, MASK_ROWY);
	}

	return pt;
}

// 测试虚拟图像中的一个坐标,返回对应的字符的行列序号. 这个逻辑一定要合符自然,否则会有各种"异常"情况.
// 首先定位行,如果超出有效范围,则返回 _curCellPos; 如果超出列的有效范围,则返回该行的最后一个字符的序号,如果该行是空行则返回 _curCellPos,它肯定是最后一个空行
// (在中间的"空行"并不是真正的空行,而是字符串"\r\n", 最后一个换行动作导致新增的一个行索引项产生的才是真正的空行,什么都没有,只是占用了一个行索引项)
// hitTest() 的逻辑不能随意变动,因为 updateWindow() 和 MyEdit 的鼠标动作依据本函数的逻辑.
int MyVirtualImage::hitTest(const POINT& pt, unsigned int* htc) const
{
	assert(pt.y >= 0);
	int pos = 0;
	unsigned int hitTestCode = 0;

	if(pt.y >= getRowInfo(_sentryRowPos, MASK_ROWY)) 
	{
		pos = _curCellPos;
		hitTestCode |= MYHTC_ROWOVF;
	}
	else
	{
		// 运行一次二分查找,从升序的行索引的y值查找出 pt.y 所在的行序号( pt.y < 哨兵y值就一定能找到一个匹配行)
		int rowPos = 0;
		int rowFrom = 0, rowTo = _sentryRowPos;
		do
		{
			int rowMiddle = rowFrom + (rowTo - rowFrom) / 2;
			if(pt.y < getRowInfo(rowFrom + 1, MASK_ROWY))
			{
				// 行位置匹配,找到所在行
				rowPos = rowFrom;

				// 确定hitTest区域
				int rowH = getRowHeight(rowPos);
				if(pt.y >= getRowInfo(rowPos, MASK_ROWY) + rowH / 2)
				{
					hitTestCode |= MYHTC_BOTTOM;
				}
				else
				{
					hitTestCode |= MYHTC_TOP;
				}
				break;
			}
			else if(pt.y < getRowInfo(rowMiddle, MASK_ROWY))
			{
				rowFrom++;
				rowTo = rowMiddle;
			}
			else
			{
				rowFrom = rowMiddle;
			}
		}while(1);

		// 根据行索引定位列序号
		POINT rng = getRowRange(rowPos);
		int rowX = 0;
		for(pos = rng.x; pos < rng.y; ++pos)
		{
			if(pt.x > (rowX + _cells[pos]->width()))
			{
				rowX += _cells[pos]->width() + _colSpacing;
			}
			else
			{
				// 测算 hitTest 区域
				if(pt.x > rowX + _cells[pos]->width() / 2)
				{
					hitTestCode |= MYHTC_RIGHT;
				}
				else
				{
					hitTestCode |= MYHTC_LEFT;
				}
				break;
			}
		}

		// 如果超出列的有效范围则返回该行的最后一个字符
		if(pos == rng.y)
		{
			hitTestCode |= MYHTC_COLOVF;
			if(rng.y > rng.x) pos = rng.y - 1;
		}
	}

	if(htc) *htc = hitTestCode;
	return pos;
}

// 行列转换为字符序号,转换结果范围 [0, _curCellPos]
int MyVirtualImage::hitTest(int row, int col, unsigned int* htc) const
{
	assert(row >= 0);
	int pos = 0;
	int hitTestCode = 0;

	do
	{
		// 超出行索引的有效范围返回最后一个字符的下一个字符的序号
		if(row >= _sentryRowPos)
		{
			pos = _curCellPos;
			hitTestCode |= MYHTC_ROWOVF;
			break;
		}

		POINT rng = getRowRange(row);
		if(col >= rng.y - rng.x)
		{
			hitTestCode |= MYHTC_COLOVF;
			if(rng.y > rng.x)
			{
				pos = rng.y - 1;
			}
			else
			{
				pos = rng.y;
			}
		}
		else
		{
			hitTestCode |= (MYHTC_LEFT | MYHTC_TOP);
			pos = rng.x + col;
		}
	}
	while(0);

	if(htc) *htc = hitTestCode;
	return pos;
}

// 设置行缓冲的内容: 换缓冲存放指定行的单元格序号,比如 _rowInfo[1] 存放第1行的第一个字符的序号
int MyVirtualImage::setRowInfo(int row, int pos, int width, int y, unsigned int mask)
{
	if(row >= _rowIndexBufLen)
	{
		assert(row == _rowIndexBufLen);

		// 重新分配行缓冲
		rowinfo_t* newRowIndex = new rowinfo_t[_rowIndexBufLen + ROWINDEX_INC];
		if(!newRowIndex)
		{
			assert(0);
			return -1;
		}
		else
		{
			// 复制内容
			memcpy(newRowIndex, _rowInfo, sizeof(rowinfo_t) * _rowIndexBufLen);
			memset(newRowIndex + _rowIndexBufLen, 0, sizeof(rowinfo_t) * ROWINDEX_INC);

			rowinfo_t* tmp = _rowInfo;
			_rowInfo = newRowIndex;
			_rowIndexBufLen += ROWINDEX_INC;

			if(tmp) delete []tmp;
		}
	}
	if(mask & MASK_ROWINDEX)
	{
		_rowInfo[row].index = pos;
	}
	if(mask & MASK_ROWWIDTH)
	{
		_rowInfo[row].width = width;
	}
	if(mask & MASK_ROWY)
	{
		_rowInfo[row].y = y;
	}
	return row;
}

int MyVirtualImage::getRowInfo(int row, unsigned int mask) const
{
	assert(row >= 0 && row < _rowIndexBufLen);
	if(mask & MASK_ROWINDEX)
	{
		return _rowInfo[row].index;
	}
	if(mask & MASK_ROWWIDTH)
	{
		return _rowInfo[row].width;
	}
	if(mask & MASK_ROWY)
	{
		return _rowInfo[row].y;
	}
	return 0;
}

int MyVirtualImage::getRowHeight(int row) const
{
	assert(row >= 0 && row < _sentryRowPos);
	return getRowInfo(row + 1, MASK_ROWY) - getRowInfo(row, MASK_ROWY);
}

// 在指定范围内搜索最长的行
int MyVirtualImage::getWidestRow(const POINT& rng) const
{
	int widest = 0;
	int widestRowPos = 0;

	for(int i = rng.x; i < rng.y; ++i)
	{
		int width = getRowInfo(i, MASK_ROWWIDTH);
		if(width > widest)
		{
			widestRowPos = i;
			widest = width;
		}
	}
	return widestRowPos;
}
/*
* 增量构建行索引 - 从 pos 位置开始添加长度为 len 的字符
* 插入新字符串时,并不是所有的行索引都要重建:
* 1. 插入位置之前的字符的行索引继续有效
* 2. 从插入所在行开始逐个填入字符,直到有一行可以容纳下需要添加的剩余的字符,则之后的行索引只要依次 +len 即可.
*
* buildRowIndex() 的效率至关重要,处理大字符串时直接影响用户体验.
* 行索引是 MyVirtualImage 效率的核心.
* 
* 2015.4.2
* updateRowIndex() 的目的是更新索引并计算出脏区域.
* 计算出总计需要填入的长度 pos * charWidth * len 
*
* 2015.4.3 - 睡前思考结果
* 1. 使用 insertLen 表示需要插入的字符的个数随着修正的进程增减直到0. (不能用 strWidth, 碰到长度为 0 的字符会有问题)
* 2. insertLen 需要包含插入行 pos 之后直到行结尾的字符.
*
* 2015.4.7
* 真正的关键核心函数非此莫属,纠结那么多天只为写出优雅,高效的实现.
* 1. updateRowIndex() 从 pos 位置开始到结尾重排,直到有一行的新序号 = 老序号 + len 就表示完成(删除时 len < 0, 这个判断条件也适用), 然后后续的行序号直接 += len就结束.

行索引的结构
[0] = 0 -> 第0行的起始序号
[1] = 7 -> 第1行的起始序号
[n] = m -> 第n行的起始序号
 .
 .
 .
[_curRowPos] = x 或者 _curCellPos. 最后一行的长度为 _curCellPos - [_curRowPos].

行结构的Y值:
[0].y = 0;
[1].y = 16; -> 第1行的起始输出Y轴位置
[2].y = 32;
 .
 .
 .
[_sentryPos].y = y 最后一行的下一行的输出位置,和上一行的y值相减得到最后一行的高度
[_sentryPos].width = 0;
[_sentryPos].index = _curCellPos; 和上一行的 index 值相减得到最后一行的长度


*
* 2015.4.9
* 1. 找出最长的行的效率问题: 3个阶段找 1. 0 ~ rowTarget; 2. rowTarget ~ rowPos; 3. rowPos ~ _curRowPos;
* 2. rowTarget ~ rowPos 中的最长行在重排过程中得到
* 3. 之后,如果原来的最长行在 0 ~ rowTarget ||  rowPos ~ _curRowPos, 则直接和步骤2的结果比较; 否则在上述范围内重新查找最长行,并和步骤2的结果比较.

* 2015.4.20
* 插入时,整个行索引分为3个部分
* 1. 目标行 rowTarget 之前不需要改动 [pos, ipos)
* 2. 目标行开始直到某个行的行首 spos 未变化 [ipos, spos)
* 3. [spos, size()) 只需要更新行索引的值
*/
int MyVirtualImage::updateRowIndex(int pos, int len)
{
	if(len == 0) return 0;

	// 最长的行3个范围
	int rowA, rowB, rowC;

	/*
	* 设置初始值,定位目标行,如果目标行已经有换行符则在下一行插入,否则在目标行的指定位置开始插入.
	*/
	// 设置初始值,定位目标行,如果目标行已经有换行符则在下一行插入,否则在目标行的指定位置开始插入.
	int rowPos = 0, rowWidth = 0, rowY = 0, rowHeight = 0;
	if(pos > 0)
	{
		// 根据插入位置的前一个字符获取目标行(前一个字符的信息因为没有变动,所以是有效的. pos 位的字符是新插入的字符,根据它不能取到有效数据)
		int colPos;
		getRowCol(pos - 1, &rowPos, &colPos);
		if(_cells[pos - 1]->wrap())
		{
			// 插入新的一行,设置新行的初始值(序号和长度 = 0),rowY = 下一行的 rowY 或者 哨兵的 rowY,所以不需要设置
			rowPos++;
			setRowInfo(rowPos, pos, 0, 0, MASK_ROWINDEX | MASK_ROWWIDTH);
		}
		else
		{
			// 目标行从 [rowRng.x, pos) 是有效的. 设置目标行的初始状态(行长度和最大高度),再开始插入.
			POINT rowRng = getRowRange(rowPos);
			for(int i = rowRng.x; i < pos; ++i)
			{
				rowWidth += _cells[i]->width() + _colSpacing;
				if(_cells[i]->height() > rowHeight) rowHeight = _cells[i]->height();
			}

			// 在目标行插入,设置目标行的初始值(序号保持不变,Y值保持不变,长度为 rowRng.x -> pos)
			setRowInfo(rowPos, 0, rowWidth, 0, MASK_ROWWIDTH);
		}
		rowY = getRowInfo(rowPos, MASK_ROWY);
	}
	else
	{
		// 目标行是第一行
		setRowInfo(rowPos, 0, 0, 0, MASK_ROWWIDTH | MASK_ROWINDEX | MASK_ROWY);
	}
	rowA = 0;
	rowB = rowPos;

	/*
	* 需要变动的部分
	*/
	// 从 pos 位置开始更新索引,直到 行的索引值 = 该行原索引值 + len
	int ipos = pos;
	int irowPos = rowPos;
	bool adj = false;
	for(ipos = pos; ipos < _curCellPos; ++ipos)		// updateRowIndex() 负责维护行索引, 它可以直接使用行索引相关的内部变量, 但是对字符数组不关心,所以使用 size() 而不是 _curCellPos
	{
		// 当前行增加一个单元格:长度增加,并记录最大高度
		rowWidth += _cells[ipos]->width();
		rowWidth += _colSpacing;
		if(_cells[ipos]->height() > rowHeight) rowHeight = _cells[ipos]->height();

		// 记录反向索引行序号
		_cells[ipos]->setRowIndex(rowPos);
		
		// 遇到换行符或者行的长度超出窗口的长度,并且自动换行标记打开时,执行换行操作
		if(_cells[ipos]->wrap())
		{
			// 记录当前行的长度
			setRowInfo(rowPos, 0, rowWidth, 0, MASK_ROWWIDTH);

			// 设置新的一行的初值(新行还是一个空行,高度初值为默认的字符高度)
			++rowPos;
			rowY += rowHeight + _rowSpacing;
			rowHeight = _charFactory.getCharHeight();
			rowWidth = 0;

			// 如果下一行的行首序号恰好等于原来的值+变更的长度len,那么说明后续不需要重新计算排列,序号+=len,然后保持原有秩序即可.
			if(rowPos < _sentryRowPos && getRowInfo(rowPos, MASK_ROWINDEX) + len == ipos + 1)
			{
				// ipos 指向第一个不需要重画的字符,调整剩余的不需要重画但是需要更新索引的行
				++ipos;
				adj = true;
				break;
			}
			else
			{
				setRowInfo(rowPos, ipos + 1, 0, rowY, MASK_ROWINDEX | MASK_ROWWIDTH | MASK_ROWY);
			}
		}
		else if(_autoWrap && _windowSize.x > 0 && rowWidth > _windowSize.x)
		{
			// 自动换行. 当前字符是新行的第一个字符,所以新行的初始值以此为基准.
			++rowPos;
			rowWidth = _cells[ipos]->width() + _colSpacing;
			rowY += rowHeight + _rowSpacing;
			rowHeight = _cells[ipos]->height();
			_cells[ipos]->setRowIndex(rowPos);

			// 下一行行首未变
			if(rowPos < _sentryRowPos && getRowInfo(rowPos, MASK_ROWINDEX) + len == ipos)
			{
				// ipos 已经指向第一个不需要重画的字符
				adj = true;
				break;
			}
			else
			{
				setRowInfo(rowPos, ipos, rowWidth, rowY, MASK_ROWINDEX | MASK_ROWWIDTH | MASK_ROWY);
			}
		}
		else
		{
			// 当前行新增一个字符,更新长度
			setRowInfo(rowPos, 0, rowWidth, 0, MASK_ROWWIDTH);
		}
	}
	
	/*
	* 后续的行在虚拟图像中不移动,只是索引值变化
	*/
	if(adj)
	{
		rowC = rowPos;

		// 后续的行包括哨兵的行索引都需要调整.(rowWidht 和 rowY 则保存不变)
		for(; rowPos <= _sentryRowPos; ++rowPos)
		{
			setRowInfo(rowPos, getRowInfo(rowPos, MASK_ROWINDEX) + len, 0, 0, MASK_ROWINDEX);
		}
		// 行数保持不变,不需要设置新的行数

		// 脏区域 [pos, ipos)
		markDirtyRange({pos, ipos});
	}
	else
	{
		// 设置哨兵的信息
		if(0 == _curCellPos)
		{
			// 最后一行是空行,就地设置为哨兵
			assert(rowPos == 0);
			_sentryRowPos = rowPos;
		}
		else
		{
			assert(ipos == _curCellPos);
			_sentryRowPos = rowPos + 1;
			rowY += rowHeight + _rowSpacing;
			setRowInfo(_sentryRowPos, ipos, 0, rowY, MASK_ROWINDEX | MASK_ROWWIDTH | MASK_ROWY);
		}

		// 标记脏区域 [pos, 可视窗口的结尾) - 重排一直到结尾,后续的空白也是脏区域
		markDirtyRange({pos, -1});

		rowC = _sentryRowPos;
	}

	/*
	* 重新查找最长的行:变动的部分分为3个范围, 变动之前行1, 变动的行2, 之后的行3.
	* 如果原最长行在范围1 或 范围3, 则和 B 比较,大者为新的最长行
	* 如果原最长行在范围2, 则计算范围1的最长行A,和范围3的最长行C 取3者的最大者.
	*/
	if(_widestRowIndex >= 0 && _widestRowIndex < rowB || _widestRowIndex >= rowC && _widestRowIndex < _sentryRowPos)
	{
		int widestRowIndexB = getWidestRow({rowB, rowC});
		int widthB = getRowInfo(widestRowIndexB, MASK_ROWWIDTH);
		if(widthB > getRowInfo(_widestRowIndex, MASK_ROWWIDTH))
		{
			_widestRowIndex = widestRowIndexB;
		}
	}
	else
	{
		_widestRowIndex = getWidestRow({0, _sentryRowPos});
	}
	
	return 0;
}

int MyVirtualImage::update(int pos, int len)
{
	return markDirtyRange({pos, pos + len});
}

int MyVirtualImage::markDirtyRange(const POINT& rng)
{
	if(rng.x == rng.y) return 0;

	if(rng.x < _dirtyRange.x || -1 == _dirtyRange.x) _dirtyRange.x = rng.x;
	if(rng.y > _dirtyRange.y || -1 == _dirtyRange.y) _dirtyRange.y = rng.y;
	return 0;
}

int MyVirtualImage::updateWindow()
{
	// 窗口区无效,不需要输出
	if(_windowSize.x <= 0 || _windowSize.y <= 0 || _dirtyRange.x == _dirtyRange.y)
	{
	}
	else
	{
		/*
		* 修正脏区域的范围
		*/
		if(_dirtyRange.x < 0) _dirtyRange.x = 0;
		if(_dirtyRange.y > _curCellPos || _dirtyRange.y < 0) _dirtyRange.y = _curCellPos;

		/*
		* 画背景
		*/
		RECT rcWindow = {0, 0, _windowSize.x, _windowSize.y};
		FillRect(_hdc, &rcWindow, _windowBkBrush);

		/*
		* 绘制可视的字符
		*/
		do
		{
			// 定位输出起始行和结束行
			int rowFrom , rowTo;
			int posFrom = hitTest({0, _windowPos.y});
			if(posFrom >= _curCellPos)
			{
				break;
			}
			rowFrom = _cells[posFrom]->getRowIndex();

			int posTo = hitTest({_windowPos.x + _windowSize.x, _windowPos.y + _windowSize.y});
			if(posTo >= _curCellPos)
			{
				rowTo = _sentryRowPos;
			}
			else
			{
				rowTo = _cells[posTo]->getRowIndex() + 1;
			}

			// 把符合范围的字符输出到内存 bmp
			int x = 0, y = 0;
			for(int i = rowFrom; i < rowTo; ++i)
			{
				// 输出一行
				x = 0 - _windowPos.x;
				y = getRowInfo(i, MASK_ROWY) - _windowPos.y;

				POINT rg = getRowRange(i);
				for(int j = rg.x; j < rg.y; ++j)
				{
					if (x + _cells[j]->width() + _colSpacing < 0)
					{
						// 整个单元完全在窗口的左边
					}
					else if(x + _cells[j]->width() > _windowSize.x)
					{
						// 整个字符完全在窗口的右边(最右边位置只要够字符本身即可,因为再往右不再输出字符,所以字间距没有意义)开始输出下一行
						break;
					}
					else
					{
						// 绘制字符
						drawc(j, {x, y});
					}
					x += _cells[j]->width();
					x += _colSpacing;
				}
			}
		}
		while(0);
	}

	// 脏区域清零
	_dirtyRange.x = -1;
	_dirtyRange.y = -1;
	return 0;
}

int MyVirtualImage::drawc(int pos, const POINT& pt)
{
	RECT rc = {pt.x, pt.y, pt.x + _cells[pos]->width(), pt.y + _cells[pos]->height()};
	_cells[pos]->draw(_hdc, &rc, pos >= _sel.x && pos < _sel.y);
	return 0;
}

int MyVirtualImage::insert(int pos, MyCell** cells, int len)
{
	// 确保空间足够
	int s = reserve(len);
	if(s < len)
	{
		s += remove(0, len - s);
	}
	len = s;

	// 找到插入位置
	if(pos > _curCellPos)
	{
		pos = _curCellPos;
	}
	else
	{
		// 移动 pos 之后的数据,空出足够的空间以插入字符
		memmove(_cells + pos + len, _cells + pos, sizeof(MyCell*) * (_curCellPos - pos));
	}

	// 填入字符,记录新的长度
	memcpy(_cells + pos, cells, sizeof(MyCell*) * len);
	_curCellPos += len;

	// 修正选区范围
	int selPos, selLen;
	getSel(&selPos, &selLen);
	if(selLen > 0)
	{
		if(pos <= selPos)
		{
			selPos += len;
		}
		else if(pos > selPos && pos <= selPos + selLen)
		{
			selLen += len;
		}
		else
		{
			// 不变
		}
		setSel(selPos, selLen);
	}

	// 更新行索引
	this->updateRowIndex(pos, len);
	return len;
}

// 在 pos 位置之前插入长度为 len 的字符串 STATICBUF_LEN_INSERT > 2048
// 
int MyVirtualImage::insert(int pos, const wchar_t* ch, int len, const COLORREF& fg, const COLORREF& bk)
{
	MyCell* staticBuf[BLOCK_SIZE];
	int inserted = 0;
	while(len > 0)
	{
		MyCellCharacter* chCells = NULL;
		int cellLen = 0;
		_charFactory.alloc(ch, len, fg, bk, &chCells, &cellLen);

		for(int i = 0; i < cellLen; ++i)
		{
			staticBuf[i] = &chCells[i];
		}
		int iret = insert(pos, staticBuf, cellLen);

		ch += cellLen;
		len -= cellLen;
		pos += iret;
		inserted += iret;
	}
	return inserted;
}

int MyVirtualImage::remove(int pos, int len)
{
	if(len == -1) len = size();
	if(len > size() - pos) len = size() - pos;

	if(pos < 0 || len <= 0) return 0;

	// 通知MyCell->remove()
	for(int i = 0; i < len; ++i)
	{
		_cells[pos + i]->remove();
	}

	// 移动数据记录新的长度
	memmove(_cells + pos, _cells + pos + len, sizeof(MyCell*) * (_curCellPos - len - pos));
	_curCellPos -= len;

	/*
	* 修正选区范围(减去重叠部分)
	*/
	int selPos, selLen;
	getSel(&selPos, &selLen);
	if(selLen > 0)
	{
		// 调整选区的起始位置
		int selPosMv = selPos - pos;
		if(selPosMv > len) selPosMv = len;
		if(selPosMv > 0) selPos -= selPosMv;

		// 调整选区的长度减去选区和删除区重合的部分(几何学:计算两条线段重合部分的长度)
		int frontPos = MIN(selPos, pos);
		int backPos = MAX(selPos + selLen, pos + len);
		if(backPos - frontPos >= selLen + len)
		{
			// 没有重叠的部分
		}
		else
		{
			selLen -= (selLen + len - (backPos - frontPos));
		}
		setSel(selPos, selLen);
	}

	// 更新行索引
	this->updateRowIndex(pos, -1 * len);
	return len;
}

MyCell* MyVirtualImage::getCell(int pos)
{
	if(pos < 0 || pos >= _curCellPos)
	{
		return NULL;
	}
	return _cells[pos];
}

const MyCell* MyVirtualImage::getCell(int pos) const
{
	if(pos < 0 || pos >= _curCellPos)
	{
		return NULL;
	}
	return _cells[pos];
}

int MyVirtualImage::getText(int pos, int len, wchar_t* dest, int destLen) const
{
	if(pos < 0 || pos >= _curCellPos) return 0;
	if(len > _curCellPos - pos) len = _curCellPos - pos;

	int destPos = 0;
	if(NULL == dest)
	{
		// 计算长度
		for(int i = 0; i < len; ++i)
		{
			destPos += _cells[pos + i]->getText(NULL, 0);
		}
	}
	else
	{
		// 实际读取
		for(int i = 0; i < len && destPos < destLen; ++i)
		{
			destPos += _cells[pos + i]->getText(dest + destPos, destLen - destPos);
		}
	}

	return destPos;
}

int MyVirtualImage::setSel(int pos, int len)
{
	// 修正参数确保选区的范围是 [0, size()]
	assert(pos >= 0 && pos <= _curCellPos);
	if(-1 == len) len = _curCellPos;
	if(pos + len > _curCellPos) len = _curCellPos - pos;
	
	// 清除原来的选中区
	markDirtyRange(_sel);

	// 计算有变化的区域,调用 MyCell::select() 或者 MyCell::unselect()
	// 采用逻辑简单的算法: 老选区内每个单元,如果它不在新选区内,则调用 unselect(); 新选区内的每个单元,若它不在老选区内,则调用 select()
	//POINT newSel = {pos, pos + len};
	//POINT oldSel = _sel;
	//for(int i = oldSel.x; i < oldSel.y; ++i)
	//{
	//	if(!is_in_range(i, newSel)) _cells[i]->unselect();
	//}
	//for(int i = newSel.x; i < newSel.y; ++i)
	//{
	//	if(!is_in_range(i, oldSel)) _cells[i]->select();
	//}

	// 重画新的选中区
	_sel = {pos, pos + len};
	markDirtyRange(_sel);
	return len;
}

int MyVirtualImage::getSel(int* pos, int* len) const
{
	*pos = _sel.x;
	*len = _sel.y - _sel.x;
	return 0;
}

int MyVirtualImage::reserve(int len)
{
	if(_cellBufLen - _curCellPos > len)
	{
		// 空间足够
	}
	else
	{
		int inc = len;
		if(inc < CELLS_INC) inc = CELLS_INC;
		if(_maxCellLen - _cellBufLen < inc) inc = _maxCellLen - _cellBufLen;
		if(inc <= 0)
		{
		}
		else
		{
			MyCell** tmp = new MyCell*[_cellBufLen + inc];
			memset(tmp, 0, sizeof(MyCell*) * (_cellBufLen + inc));
			if(_cells)
			{
				memcpy(tmp, _cells, sizeof(MyCell*) * _curCellPos);
				delete []_cells;
			}
			_cells = tmp;
			_cellBufLen += inc;
		}
		
		if(len > _cellBufLen - _curCellPos) len = _cellBufLen - _curCellPos;
	}
	return len;
}

// 输出可视窗口的图像
int MyVirtualImage::render(HDC hdc, const RECT& dest)
{
	// 更新虚拟图像
	updateWindow();

	// 把内存bmp贴到指定位置
	BitBlt(hdc, dest.left, dest.top, (dest.right - dest.left), (dest.bottom - dest.top), _hdc, 0, 0, SRCCOPY);
	return 0;
}

int MyVirtualImage::setWindowSize(const POINT& sz)
{
	if(_windowSize.x == sz.x && _windowSize.y == sz.y) return 0;
	_windowSize = sz;

	if(_windowSize.x == 0 || _windowSize.y == 0)
	{
		// 无效的窗口大小
		return 0;
	}

	/*
	* 这里一定要取到屏幕或者窗口的DC,不能用内存DC作为 CreateCompatibleBitmap 的参数
	* 因为内存DC创建后是单色的,只有一个像素大小,如果以此为参数创建相应的 bmp,那么 bmp
	* 也是单色的. 这个问题差点坑到我.(2015.1.25)
	*/
	if(_hbmp) DeleteObject(_hbmp);
	_hbmp = NULL;

	HDC screenDc = GetWindowDC(NULL);
	_hbmp = CreateCompatibleBitmap(screenDc, _windowSize.x, _windowSize.y);
	SelectObject(_hdc, (HGDIOBJ)_hbmp);
	ReleaseDC(NULL, screenDc);

	// 改变宽度的大小会导致重新排列(自动换行打开的话)
	// 重新生成行索引
	rebuildRowIndex();

	// 重画
	redraw();
	return 0;
}

/*
* 在内存DC内重画
*/
int MyVirtualImage::redraw()
{
	// 整个可视窗口标记为脏区域
	markDirtyRange({0, -1});
	return 0;
}

int MyVirtualImage::setWindowPos(const POINT& pos)
{
	if(pos.x == _windowPos.x && pos.y == _windowPos.y) return 1;

	_windowPos.x = pos.x;
	_windowPos.y = pos.y;

	redraw();
	return 0;
}

POINT MyVirtualImage::getImageSize() const
{
	return {getRowInfo(_widestRowIndex, MASK_ROWWIDTH), getRowInfo(_sentryRowPos, MASK_ROWY)};
}

int MyVirtualImage::rebuildRowIndex()
{
	setRowInfo(0, 0, 0, 0, MASK_ROWINDEX | MASK_ROWWIDTH | MASK_ROWY);
	_sentryRowPos = 0;
	return updateRowIndex(0, size());
}

bool MyVirtualImage::enableAutoWrap(bool autoWrap)
{
	bool oldVal = _autoWrap;
	if(autoWrap != _autoWrap)
	{
		// 切换自动换行模式后要重画
		_autoWrap = autoWrap;
		rebuildRowIndex();
		redraw();
	}

	return oldVal;
}
