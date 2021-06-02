#include "stdafx.h"
#include <assert.h>
#include <time.h>
#include "MyCell.h"

/*///////////////////////////////////////////////////////////////////////////////////
* �������� 
*////////////////////////////////////////////////////////////////////////////////////
#define ROWINDEX_INC 200			// �л��岻��ʱ�����ĸ���
#define CHARS_INC 1024				// �ַ����岻��ʱ�����ĸ���
#define CELLS_INC 1024				// ��Ԫ�񻺳岻���������ĸ���

#define BLOCK_SIZE 2048
#define BLOCK_INDEX_INC 64

// ��������Ϣ��������
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
* ȫ�ֺ���
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
		// ��ѡ����,����һ������ɫ
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
		// ��ѡ����
		SetTextColor(hDc, _fgClr);
		SetBkColor(hDc, _bkClr);
	}

	if(_width <= 0 || _height <= 0 || L'\t' == _ch)
	{
	}
	else
	{
		// ����ı�
		TextOut(hDc, rc->left, rc->top, &_ch, 1);

		// �ı��������һ������
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
	// ��¼����
	_hdc = hdc;
	_hFont = font;

	// ��ȡ������ص���Ϣ
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
		// Ĭ���и� 16 ����
		_charHeight = 16;
	}
	else if(_charHeight > 0)
	{
	}
	else
	{
		_charHeight *= -1;
	}

	// ��������MyCellCharacter ���ַ���Ⱥ͸߶�
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

		// ��һ�η����ַ���֮ǰ��ʼ��֮
		if(_blocks[_curBlock].buf == NULL)
		{
			assert(_curIndex == 0);
			assert(_blocks[_curBlock].ref == 0);
			_blocks[_curBlock].buf = new MyCellCharacter[_blockSize];
		}

		// ֱ�Ӱѵ�ǰ�ַ���ʣ����ַ�����,���ȿ���С�� len
		rlen = _blockSize - _curIndex;
		if(rlen > len) rlen = len;

		*buf = _blocks[_curBlock].buf + _curIndex;
		_curIndex += rlen;
		_blocks[_curBlock].ref += rlen;

		assert(_blocks[_curBlock].ref <= _curIndex);

		// ��ǰ�ַ����Ѿ�����
		if(_curIndex >= _blockSize)
		{
			_curBlock++;
			_curIndex = 0;
		}
	}
	else
	{
		// ��ǰ�ַ����Ѿ�����,����һ���µ��ַ���
		// 1. ȷ���ַ�����������
		charsblock_t* tmp = new charsblock_t[_blocksLen + BLOCK_INDEX_INC];
		memset(tmp, 0, sizeof(charsblock_t) * (_blocksLen + BLOCK_INDEX_INC));
		if(_blocks)
		{
			memcpy(tmp, _blocks, sizeof(charsblock_t) * _blocksLen);
			delete []_blocks;
		}
		_blocks = tmp;
		_blocksLen += BLOCK_INDEX_INC;

		// 2. �����ַ����з���
		return reserve(len, buf);
	}
	return rlen;
}

int MyCellCharacterFactory::alloc(const wchar_t* ch, int len, COLORREF fgClr, COLORREF bkClr, MyCellCharacter** chCells, int* cellLen)
{
	// ����������������ڴ�,����ڴ�������ʹ�� MyCellCharacterFactory �� MyVirtualImage ����
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
	// �ҵ��ַ���Ӧ���ַ���
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

		// ���ַ�������ü��� - 1
		blockPtr->ref--;

		// ����ַ������(�����ü��� = 0)��Ѹ��ַ����ƶ�����β,׼���´�ʹ��
		if(0 == blockPtr->ref)
		{
			assert(blockIdx <= _curBlock);

			if(blockIdx == _curBlock)
			{
				// �ͷŵ��ַ�����ǵ�ǰ�ַ���,��� _curIndex ����ͱ�ʾ�ַ����ֿ�����.
				_curIndex = 0;
			}
			else if(blockIdx == _curBlock - 1)
			{
				// �ͷŵ��ַ����ǵ�ǰ�ַ����ǰһ��,ֻҪ��һ�ν���
				swapBlock(blockIdx, _curBlock);
				_curBlock--;
			}
			else
			{
				// ���������Ҫ�����ν���:�Ȱѿ����ַ��齻������ǰ���ǰһ��λ��,Ȼ���ٰѵ�ǰ���ǰһ�齻��
				swapBlock(blockIdx, _curBlock - 1);
				swapBlock(_curBlock - 1, _curBlock);
				_curBlock--;
			}

			// ������е��ַ���̫����,��ɾ��һЩ
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
	// Ĭ�ϵ������Ǻ�ϵͳ����̨����һ���ĵȿ�����
	_hdc = CreateCompatibleDC(hdc);

	// ��������ĳ�ʼ״̬-δ����
	_rowSpacing = 0;
	_colSpacing = 0;
	_alignMode = MYALIGN_TOP;
	_autoWrap = autoWrap;

	// ��ʼ���ڵĴ�С��λ��
	_hbmp = NULL;
	_windowPos.x = 0;
	_windowPos.y = 0;
	_windowSize.x = 0;
	_windowSize.y = 0;
	_windowBkBrush = NULL;
	_sel.x = 0;
	_sel.y = 0;
	_dirtyRange.x = 0;		// ���������ʼ״̬�������
	_dirtyRange.y = -1;

	// �ַ�����
	_cells = NULL;
	_curCellPos = 0;
	_cellBufLen = 0;
	_maxCellLen = maxChars;
	
	// ��ʼ�л���,д��ʱ����� setRowInfo ���Զ�ά���л��������
	_rowInfo = NULL;
	_rowIndexBufLen = 0;
	_widestRowIndex = 0;
	_sentryRowPos = 0;
	setRowInfo(0, 0, 0, 0, MASK_ROWWIDTH | MASK_ROWINDEX | MASK_ROWY);
	
	// ����Ĭ�ϸ�ʽ SYSTEM_FIXED_FONT / DEFAULT_GUI_FONT
	setFont((HFONT)GetStockObject(SYSTEM_FIXED_FONT), 0, 0, 4);
	setBkBrush((HBRUSH)GetStockObject(BLACK_BRUSH));
	//setBkMode(OPAQUE);  // Ĭ��ģʽ���� OPAQUE
	//setSelColour(RGB(173,214,255));

	// �����ַ�����
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
	// 1. ��¼����
	_colSpacing = colSpacing;
	_rowSpacing = rowSpacing;

	// 2. ѡ������,���������ַ��Ŀ��
	HFONT oldFont = (HFONT)SelectObject(_hdc, font);
	_charFactory.setFont(_hdc, font, tabSize);
	
	// 3. �����µĲ����ؽ�������֮���ػ�
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

// ��ȡ�з�Χ: �����ڱ��Ĵ���,���ǿ���ͨ����ȡ��һ�е���ʼλ�������㺬��Χ.
POINT MyVirtualImage::getRowRange(int row) const
{
	assert(row >= 0 && row < _sentryRowPos);
	return {getRowInfo(row, MASK_ROWINDEX), getRowInfo(row + 1, MASK_ROWINDEX)};
}

// �ַ���� -> �������.���ڵ�Ԫ�������һ�������������Կ���ֱ�ӵõ������,������Ҫ�ö��ֲ����㷨ɨ��������ֱ���ҵ���Ӧ��ŵ���.
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

// �ַ���� -> ����ͼ���������(leftTop)
// pos ����Ч��Χ�� [0, _curCellPos] �� pos = _curCellPos ʱ�������һ���ַ��������λ��,����һ�������������;
POINT MyVirtualImage::getPoint(int pos) const
{
	assert(pos >= 0 && pos <= _curCellPos);
	POINT pt = {0, 0};
	if(pos >= _curCellPos)
	{
		// �������һ���ַ�����һ���ַ������λ��
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
		// ���ض�Ӧ�ַ���λ��
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

// ��������ͼ���е�һ������,���ض�Ӧ���ַ����������. ����߼�һ��Ҫ�Ϸ���Ȼ,������и���"�쳣"���.
// ���ȶ�λ��,���������Ч��Χ,�򷵻� _curCellPos; ��������е���Ч��Χ,�򷵻ظ��е����һ���ַ������,��������ǿ����򷵻� _curCellPos,���϶������һ������
// (���м��"����"�����������Ŀ���,�����ַ���"\r\n", ���һ�����ж�������������һ��������������Ĳ��������Ŀ���,ʲô��û��,ֻ��ռ����һ����������)
// hitTest() ���߼���������䶯,��Ϊ updateWindow() �� MyEdit ����궯�����ݱ��������߼�.
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
		// ����һ�ζ��ֲ���,���������������yֵ���ҳ� pt.y ���ڵ������( pt.y < �ڱ�yֵ��һ�����ҵ�һ��ƥ����)
		int rowPos = 0;
		int rowFrom = 0, rowTo = _sentryRowPos;
		do
		{
			int rowMiddle = rowFrom + (rowTo - rowFrom) / 2;
			if(pt.y < getRowInfo(rowFrom + 1, MASK_ROWY))
			{
				// ��λ��ƥ��,�ҵ�������
				rowPos = rowFrom;

				// ȷ��hitTest����
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

		// ������������λ�����
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
				// ���� hitTest ����
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

		// ��������е���Ч��Χ�򷵻ظ��е����һ���ַ�
		if(pos == rng.y)
		{
			hitTestCode |= MYHTC_COLOVF;
			if(rng.y > rng.x) pos = rng.y - 1;
		}
	}

	if(htc) *htc = hitTestCode;
	return pos;
}

// ����ת��Ϊ�ַ����,ת�������Χ [0, _curCellPos]
int MyVirtualImage::hitTest(int row, int col, unsigned int* htc) const
{
	assert(row >= 0);
	int pos = 0;
	int hitTestCode = 0;

	do
	{
		// ��������������Ч��Χ�������һ���ַ�����һ���ַ������
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

// �����л��������: ��������ָ���еĵ�Ԫ�����,���� _rowInfo[1] ��ŵ�1�еĵ�һ���ַ������
int MyVirtualImage::setRowInfo(int row, int pos, int width, int y, unsigned int mask)
{
	if(row >= _rowIndexBufLen)
	{
		assert(row == _rowIndexBufLen);

		// ���·����л���
		rowinfo_t* newRowIndex = new rowinfo_t[_rowIndexBufLen + ROWINDEX_INC];
		if(!newRowIndex)
		{
			assert(0);
			return -1;
		}
		else
		{
			// ��������
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

// ��ָ����Χ�����������
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
* �������������� - �� pos λ�ÿ�ʼ��ӳ���Ϊ len ���ַ�
* �������ַ���ʱ,���������е���������Ҫ�ؽ�:
* 1. ����λ��֮ǰ���ַ���������������Ч
* 2. �Ӳ��������п�ʼ��������ַ�,ֱ����һ�п�����������Ҫ��ӵ�ʣ����ַ�,��֮���������ֻҪ���� +len ����.
*
* buildRowIndex() ��Ч��������Ҫ,������ַ���ʱֱ��Ӱ���û�����.
* �������� MyVirtualImage Ч�ʵĺ���.
* 
* 2015.4.2
* updateRowIndex() ��Ŀ���Ǹ��������������������.
* ������ܼ���Ҫ����ĳ��� pos * charWidth * len 
*
* 2015.4.3 - ˯ǰ˼�����
* 1. ʹ�� insertLen ��ʾ��Ҫ������ַ��ĸ������������Ľ�������ֱ��0. (������ strWidth, ��������Ϊ 0 ���ַ���������)
* 2. insertLen ��Ҫ���������� pos ֮��ֱ���н�β���ַ�.
*
* 2015.4.7
* �����Ĺؼ����ĺ����Ǵ�Ī��,������ô����ֻΪд������,��Ч��ʵ��.
* 1. updateRowIndex() �� pos λ�ÿ�ʼ����β����,ֱ����һ�е������ = ����� + len �ͱ�ʾ���(ɾ��ʱ len < 0, ����ж�����Ҳ����), Ȼ������������ֱ�� += len�ͽ���.

�������Ľṹ
[0] = 0 -> ��0�е���ʼ���
[1] = 7 -> ��1�е���ʼ���
[n] = m -> ��n�е���ʼ���
 .
 .
 .
[_curRowPos] = x ���� _curCellPos. ���һ�еĳ���Ϊ _curCellPos - [_curRowPos].

�нṹ��Yֵ:
[0].y = 0;
[1].y = 16; -> ��1�е���ʼ���Y��λ��
[2].y = 32;
 .
 .
 .
[_sentryPos].y = y ���һ�е���һ�е����λ��,����һ�е�yֵ����õ����һ�еĸ߶�
[_sentryPos].width = 0;
[_sentryPos].index = _curCellPos; ����һ�е� index ֵ����õ����һ�еĳ���


*
* 2015.4.9
* 1. �ҳ�����е�Ч������: 3���׶��� 1. 0 ~ rowTarget; 2. rowTarget ~ rowPos; 3. rowPos ~ _curRowPos;
* 2. rowTarget ~ rowPos �е���������Ź����еõ�
* 3. ֮��,���ԭ��������� 0 ~ rowTarget ||  rowPos ~ _curRowPos, ��ֱ�ӺͲ���2�Ľ���Ƚ�; ������������Χ�����²������,���Ͳ���2�Ľ���Ƚ�.

* 2015.4.20
* ����ʱ,������������Ϊ3������
* 1. Ŀ���� rowTarget ֮ǰ����Ҫ�Ķ� [pos, ipos)
* 2. Ŀ���п�ʼֱ��ĳ���е����� spos δ�仯 [ipos, spos)
* 3. [spos, size()) ֻ��Ҫ������������ֵ
*/
int MyVirtualImage::updateRowIndex(int pos, int len)
{
	if(len == 0) return 0;

	// �����3����Χ
	int rowA, rowB, rowC;

	/*
	* ���ó�ʼֵ,��λĿ����,���Ŀ�����Ѿ��л��з�������һ�в���,������Ŀ���е�ָ��λ�ÿ�ʼ����.
	*/
	// ���ó�ʼֵ,��λĿ����,���Ŀ�����Ѿ��л��з�������һ�в���,������Ŀ���е�ָ��λ�ÿ�ʼ����.
	int rowPos = 0, rowWidth = 0, rowY = 0, rowHeight = 0;
	if(pos > 0)
	{
		// ���ݲ���λ�õ�ǰһ���ַ���ȡĿ����(ǰһ���ַ�����Ϣ��Ϊû�б䶯,��������Ч��. pos λ���ַ����²�����ַ�,����������ȡ����Ч����)
		int colPos;
		getRowCol(pos - 1, &rowPos, &colPos);
		if(_cells[pos - 1]->wrap())
		{
			// �����µ�һ��,�������еĳ�ʼֵ(��źͳ��� = 0),rowY = ��һ�е� rowY ���� �ڱ��� rowY,���Բ���Ҫ����
			rowPos++;
			setRowInfo(rowPos, pos, 0, 0, MASK_ROWINDEX | MASK_ROWWIDTH);
		}
		else
		{
			// Ŀ���д� [rowRng.x, pos) ����Ч��. ����Ŀ���еĳ�ʼ״̬(�г��Ⱥ����߶�),�ٿ�ʼ����.
			POINT rowRng = getRowRange(rowPos);
			for(int i = rowRng.x; i < pos; ++i)
			{
				rowWidth += _cells[i]->width() + _colSpacing;
				if(_cells[i]->height() > rowHeight) rowHeight = _cells[i]->height();
			}

			// ��Ŀ���в���,����Ŀ���еĳ�ʼֵ(��ű��ֲ���,Yֵ���ֲ���,����Ϊ rowRng.x -> pos)
			setRowInfo(rowPos, 0, rowWidth, 0, MASK_ROWWIDTH);
		}
		rowY = getRowInfo(rowPos, MASK_ROWY);
	}
	else
	{
		// Ŀ�����ǵ�һ��
		setRowInfo(rowPos, 0, 0, 0, MASK_ROWWIDTH | MASK_ROWINDEX | MASK_ROWY);
	}
	rowA = 0;
	rowB = rowPos;

	/*
	* ��Ҫ�䶯�Ĳ���
	*/
	// �� pos λ�ÿ�ʼ��������,ֱ�� �е�����ֵ = ����ԭ����ֵ + len
	int ipos = pos;
	int irowPos = rowPos;
	bool adj = false;
	for(ipos = pos; ipos < _curCellPos; ++ipos)		// updateRowIndex() ����ά��������, ������ֱ��ʹ����������ص��ڲ�����, ���Ƕ��ַ����鲻����,����ʹ�� size() ������ _curCellPos
	{
		// ��ǰ������һ����Ԫ��:��������,����¼���߶�
		rowWidth += _cells[ipos]->width();
		rowWidth += _colSpacing;
		if(_cells[ipos]->height() > rowHeight) rowHeight = _cells[ipos]->height();

		// ��¼�������������
		_cells[ipos]->setRowIndex(rowPos);
		
		// �������з������еĳ��ȳ������ڵĳ���,�����Զ����б�Ǵ�ʱ,ִ�л��в���
		if(_cells[ipos]->wrap())
		{
			// ��¼��ǰ�еĳ���
			setRowInfo(rowPos, 0, rowWidth, 0, MASK_ROWWIDTH);

			// �����µ�һ�еĳ�ֵ(���л���һ������,�߶ȳ�ֵΪĬ�ϵ��ַ��߶�)
			++rowPos;
			rowY += rowHeight + _rowSpacing;
			rowHeight = _charFactory.getCharHeight();
			rowWidth = 0;

			// �����һ�е��������ǡ�õ���ԭ����ֵ+����ĳ���len,��ô˵����������Ҫ���¼�������,���+=len,Ȼ�󱣳�ԭ�����򼴿�.
			if(rowPos < _sentryRowPos && getRowInfo(rowPos, MASK_ROWINDEX) + len == ipos + 1)
			{
				// ipos ָ���һ������Ҫ�ػ����ַ�,����ʣ��Ĳ���Ҫ�ػ�������Ҫ������������
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
			// �Զ�����. ��ǰ�ַ������еĵ�һ���ַ�,�������еĳ�ʼֵ�Դ�Ϊ��׼.
			++rowPos;
			rowWidth = _cells[ipos]->width() + _colSpacing;
			rowY += rowHeight + _rowSpacing;
			rowHeight = _cells[ipos]->height();
			_cells[ipos]->setRowIndex(rowPos);

			// ��һ������δ��
			if(rowPos < _sentryRowPos && getRowInfo(rowPos, MASK_ROWINDEX) + len == ipos)
			{
				// ipos �Ѿ�ָ���һ������Ҫ�ػ����ַ�
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
			// ��ǰ������һ���ַ�,���³���
			setRowInfo(rowPos, 0, rowWidth, 0, MASK_ROWWIDTH);
		}
	}
	
	/*
	* ��������������ͼ���в��ƶ�,ֻ������ֵ�仯
	*/
	if(adj)
	{
		rowC = rowPos;

		// �������а����ڱ�������������Ҫ����.(rowWidht �� rowY �򱣴治��)
		for(; rowPos <= _sentryRowPos; ++rowPos)
		{
			setRowInfo(rowPos, getRowInfo(rowPos, MASK_ROWINDEX) + len, 0, 0, MASK_ROWINDEX);
		}
		// �������ֲ���,����Ҫ�����µ�����

		// ������ [pos, ipos)
		markDirtyRange({pos, ipos});
	}
	else
	{
		// �����ڱ�����Ϣ
		if(0 == _curCellPos)
		{
			// ���һ���ǿ���,�͵�����Ϊ�ڱ�
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

		// ��������� [pos, ���Ӵ��ڵĽ�β) - ����һֱ����β,�����Ŀհ�Ҳ��������
		markDirtyRange({pos, -1});

		rowC = _sentryRowPos;
	}

	/*
	* ���²��������:�䶯�Ĳ��ַ�Ϊ3����Χ, �䶯֮ǰ��1, �䶯����2, ֮�����3.
	* ���ԭ����ڷ�Χ1 �� ��Χ3, ��� B �Ƚ�,����Ϊ�µ����
	* ���ԭ����ڷ�Χ2, ����㷶Χ1�����A,�ͷ�Χ3�����C ȡ3�ߵ������.
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
	// ��������Ч,����Ҫ���
	if(_windowSize.x <= 0 || _windowSize.y <= 0 || _dirtyRange.x == _dirtyRange.y)
	{
	}
	else
	{
		/*
		* ����������ķ�Χ
		*/
		if(_dirtyRange.x < 0) _dirtyRange.x = 0;
		if(_dirtyRange.y > _curCellPos || _dirtyRange.y < 0) _dirtyRange.y = _curCellPos;

		/*
		* ������
		*/
		RECT rcWindow = {0, 0, _windowSize.x, _windowSize.y};
		FillRect(_hdc, &rcWindow, _windowBkBrush);

		/*
		* ���ƿ��ӵ��ַ�
		*/
		do
		{
			// ��λ�����ʼ�кͽ�����
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

			// �ѷ��Ϸ�Χ���ַ�������ڴ� bmp
			int x = 0, y = 0;
			for(int i = rowFrom; i < rowTo; ++i)
			{
				// ���һ��
				x = 0 - _windowPos.x;
				y = getRowInfo(i, MASK_ROWY) - _windowPos.y;

				POINT rg = getRowRange(i);
				for(int j = rg.x; j < rg.y; ++j)
				{
					if (x + _cells[j]->width() + _colSpacing < 0)
					{
						// ������Ԫ��ȫ�ڴ��ڵ����
					}
					else if(x + _cells[j]->width() > _windowSize.x)
					{
						// �����ַ���ȫ�ڴ��ڵ��ұ�(���ұ�λ��ֻҪ���ַ�������,��Ϊ�����Ҳ�������ַ�,�����ּ��û������)��ʼ�����һ��
						break;
					}
					else
					{
						// �����ַ�
						drawc(j, {x, y});
					}
					x += _cells[j]->width();
					x += _colSpacing;
				}
			}
		}
		while(0);
	}

	// ����������
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
	// ȷ���ռ��㹻
	int s = reserve(len);
	if(s < len)
	{
		s += remove(0, len - s);
	}
	len = s;

	// �ҵ�����λ��
	if(pos > _curCellPos)
	{
		pos = _curCellPos;
	}
	else
	{
		// �ƶ� pos ֮�������,�ճ��㹻�Ŀռ��Բ����ַ�
		memmove(_cells + pos + len, _cells + pos, sizeof(MyCell*) * (_curCellPos - pos));
	}

	// �����ַ�,��¼�µĳ���
	memcpy(_cells + pos, cells, sizeof(MyCell*) * len);
	_curCellPos += len;

	// ����ѡ����Χ
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
			// ����
		}
		setSel(selPos, selLen);
	}

	// ����������
	this->updateRowIndex(pos, len);
	return len;
}

// �� pos λ��֮ǰ���볤��Ϊ len ���ַ��� STATICBUF_LEN_INSERT > 2048
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

	// ֪ͨMyCell->remove()
	for(int i = 0; i < len; ++i)
	{
		_cells[pos + i]->remove();
	}

	// �ƶ����ݼ�¼�µĳ���
	memmove(_cells + pos, _cells + pos + len, sizeof(MyCell*) * (_curCellPos - len - pos));
	_curCellPos -= len;

	/*
	* ����ѡ����Χ(��ȥ�ص�����)
	*/
	int selPos, selLen;
	getSel(&selPos, &selLen);
	if(selLen > 0)
	{
		// ����ѡ������ʼλ��
		int selPosMv = selPos - pos;
		if(selPosMv > len) selPosMv = len;
		if(selPosMv > 0) selPos -= selPosMv;

		// ����ѡ���ĳ��ȼ�ȥѡ����ɾ�����غϵĲ���(����ѧ:���������߶��غϲ��ֵĳ���)
		int frontPos = MIN(selPos, pos);
		int backPos = MAX(selPos + selLen, pos + len);
		if(backPos - frontPos >= selLen + len)
		{
			// û���ص��Ĳ���
		}
		else
		{
			selLen -= (selLen + len - (backPos - frontPos));
		}
		setSel(selPos, selLen);
	}

	// ����������
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
		// ���㳤��
		for(int i = 0; i < len; ++i)
		{
			destPos += _cells[pos + i]->getText(NULL, 0);
		}
	}
	else
	{
		// ʵ�ʶ�ȡ
		for(int i = 0; i < len && destPos < destLen; ++i)
		{
			destPos += _cells[pos + i]->getText(dest + destPos, destLen - destPos);
		}
	}

	return destPos;
}

int MyVirtualImage::setSel(int pos, int len)
{
	// ��������ȷ��ѡ���ķ�Χ�� [0, size()]
	assert(pos >= 0 && pos <= _curCellPos);
	if(-1 == len) len = _curCellPos;
	if(pos + len > _curCellPos) len = _curCellPos - pos;
	
	// ���ԭ����ѡ����
	markDirtyRange(_sel);

	// �����б仯������,���� MyCell::select() ���� MyCell::unselect()
	// �����߼��򵥵��㷨: ��ѡ����ÿ����Ԫ,�����������ѡ����,����� unselect(); ��ѡ���ڵ�ÿ����Ԫ,����������ѡ����,����� select()
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

	// �ػ��µ�ѡ����
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
		// �ռ��㹻
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

// ������Ӵ��ڵ�ͼ��
int MyVirtualImage::render(HDC hdc, const RECT& dest)
{
	// ��������ͼ��
	updateWindow();

	// ���ڴ�bmp����ָ��λ��
	BitBlt(hdc, dest.left, dest.top, (dest.right - dest.left), (dest.bottom - dest.top), _hdc, 0, 0, SRCCOPY);
	return 0;
}

int MyVirtualImage::setWindowSize(const POINT& sz)
{
	if(_windowSize.x == sz.x && _windowSize.y == sz.y) return 0;
	_windowSize = sz;

	if(_windowSize.x == 0 || _windowSize.y == 0)
	{
		// ��Ч�Ĵ��ڴ�С
		return 0;
	}

	/*
	* ����һ��Ҫȡ����Ļ���ߴ��ڵ�DC,�������ڴ�DC��Ϊ CreateCompatibleBitmap �Ĳ���
	* ��Ϊ�ڴ�DC�������ǵ�ɫ��,ֻ��һ�����ش�С,����Դ�Ϊ����������Ӧ�� bmp,��ô bmp
	* Ҳ�ǵ�ɫ��. ���������ӵ���.(2015.1.25)
	*/
	if(_hbmp) DeleteObject(_hbmp);
	_hbmp = NULL;

	HDC screenDc = GetWindowDC(NULL);
	_hbmp = CreateCompatibleBitmap(screenDc, _windowSize.x, _windowSize.y);
	SelectObject(_hdc, (HGDIOBJ)_hbmp);
	ReleaseDC(NULL, screenDc);

	// �ı��ȵĴ�С�ᵼ����������(�Զ����д򿪵Ļ�)
	// ��������������
	rebuildRowIndex();

	// �ػ�
	redraw();
	return 0;
}

/*
* ���ڴ�DC���ػ�
*/
int MyVirtualImage::redraw()
{
	// �������Ӵ��ڱ��Ϊ������
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
		// �л��Զ�����ģʽ��Ҫ�ػ�
		_autoWrap = autoWrap;
		rebuildRowIndex();
		redraw();
	}

	return oldVal;
}
