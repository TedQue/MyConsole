#pragma once

/* 2015.6.12 - ʵ��˼·����

����ͼ����һϵ�еĵ�Ԫ�����к����,��Ԫ��������ַ����������κ�֧�� MyCell �ӿڵ���.
���ͼ����һ������ͼ��,ֻ��ĳ����ʽ����Ĵ������ڴ���,������ʾ���ڷ�Χ�ڵĲ���.

1. ���е�Ԫ��洢��һ��һά������.
2. ʹ��һ��������ͨ����¼ÿ�е���ʼλ�ð�������������ͳɶ�ά����.
3. �����ά��Ԫ�����鰴���������������������������ͼ��,����ͼ��ĸ�����ʵ�ֵĻ���.
4. ������ʾ������ֻ������ͼ���е�һ���ֻ���ȫ��,ͨ��������Եó���Ҫ��ʾ�ĵ�Ԫ��,��ʾʱ,�Ȱ���Ҫ��ʾ�ĵ�Ԫ�������һ���ڴ�BMP,Ȼ���ٰ����BMP��������.
5. ��������,ÿ��ռ��һ��,��¼���� index ��ʼ���, width �п��,���е�����ͼ���е�Y�� y ��λ��,�����������һ���ڱ���.
6. �ڱ��� index = ���е�Ԫ��ĸ���, width = 0, y = ����ͼ��ĸ߶�. �ڱ���Ĵ���ʹ�����ǿ����õ� n + 1 �е������� �͵� n �е�����������õ��� n �еĵ�Ԫ����ŷ�Χ,�и߶�.
7. ���������ݵ�Ԫ�����Ϣ����,����ʱ,��������ԭ�е���Ϣ. MyVirtualImage::updateRowIndex() �����е�ע��˵������ξ���������ԭ�нṹά��������.
8. ÿ����Ԫ�񱣴�õ�Ԫ��������ͼ���е������,��Ϊ��������.
9. �û�ͨ��(��,��),������������������ͼ��; �ڲ�ʹ��һά��������(����Ԫ�����)����������ͼ��.ͨ���������ͷ�������������3������ϵ��ת��.

�ܽ��ʱ��,��ô���仰��˵����,ʵ�ʱ������д�����ϸ��������Ҫ����,�����Ǹ����ٽ����.

*/

/*
* ��Ԫ��ӿ�
*/
class MyCell
{
private:
	int _rowIndex;

protected:
	MyCell() {};
	virtual ~MyCell() {};

public:
	// ����������
	void setRowIndex(int idx) { _rowIndex = idx; }
	int getRowIndex() const {return _rowIndex; }

	// �ػ�ʱ�ɿ�ܵ���(ʹ��MyVirtualImage����ϵ)
	virtual void draw(HDC hDc, const RECT* rc, bool selected) = 0;

	// ��Ԫ���MyVirtualImage �б�ɾ��ʱ�ɿ�ܵ���
	virtual void remove() = 0;

	// ��ȡ��Ԫ����ı�(str == NULL ʱ���㳤��)
	virtual int getText(wchar_t* str, int len) const = 0;

	// ��ʾ��Ԫ����Ƿ���Ҫ����
	virtual int width() const = 0;
	virtual int height() const = 0;
	virtual bool wrap() const = 0;
};

// �ַ���Ԫ��ʵ��
class MyCellCharacterFactory;
class MyCellCharacter : public MyCell
{
protected:
	MyCellCharacterFactory* _ownerFactory;
	int _chIndex;		// �� MyCellCharacterFactory �е����
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
* �ַ���Ԫ�񹤳� - ����ַ���Ԫ�������,����Ƶ�� new delete,ͳһ��������������ַ���Ԫ���ܹ������Դ
* ��������,�ַ���ص�������Ϣ
*/
class MyCellCharacterFactory
{
private:
	typedef struct
	{
		MyCellCharacter* buf;	// �ַ���ĵ�ַ,����Ϊ CHARSBUF_BLOCK_LEN
		int ref;				// ���ַ�������ü��� ref = 0 ����, ref = CHARSBUF_BLOCK_LEN ȫ����ռ��
	}charsblock_t;
	charsblock_t* _blocks;		// �ַ��黺��
	int _blockSize;				// ÿ���ַ���Ĵ�С
	int _blocksLen;				// �ַ��黺�峤��
	int _curBlock;				// ָ��ǰ�ַ���
	int _curIndex;				// ָ��ǰ�ַ���ĵ�һ�������ַ�λ��

	HDC _hdc;
	HFONT _hFont;
	int _charHeight;			// �и�
	int _charAveWidth;			// �ַ���ƽ����Ȳο�ֵ
	int _tabWidth;				// TAB �Ŀ��(����)
	int getCharWidth(wchar_t ch) const;						// ��ȡһ���ַ��Ŀ��
	int reserve(int len, MyCellCharacter** buf);		// ����len���ַ�
	int findBlock(MyCellCharacter* ch);
	void swapBlock(int i, int j);			// ����λ��i �� λ��j ���ַ���Ԫ
public:
	MyCellCharacterFactory(int blockSize);
	virtual ~MyCellCharacterFactory();

	void setFont(HDC hdc, HFONT font, int tabSize);
	int getCharAveWidth() const;
	int getCharHeight() const;

	/*
	* Ϊ�����Ч��,��"�����"�ķ�ʽ�����ַ���Ԫ.
	*/
	int alloc(const wchar_t* ch, int len, COLORREF fgClr, COLORREF bkClr, MyCellCharacter** chCells, int* cellLen);
	void recycle(MyCellCharacter** chs, int len);
};

/*
* ����ͼ��ʵ�� MyVirtualImage
*/
// ���ڵĶ��뷽ʽ
#define MYALIGN_TOP 0
#define MYALIGN_CENTER 1
#define MYALIGN_BOTTOM 2

// ͨ���������λ��ʱ���ص����� hitTest()
#define MYHTC_LEFT		0x0001
#define MYHTC_RIGHT		0x0002
#define MYHTC_TOP		0x0010
#define MYHTC_BOTTOM	0x0020
#define MYHTC_ROWOVF	0x0100		// row overflow
#define MYHTC_COLOVF	0X0200		// column overflow

class MyVirtualImage
{
private:
	MyCell** _cells;			// ������(��һά����ģ���ά����,���������޸��к���.
	int _cellBufLen;			// ���峤��
	int _curCellPos;			// ָ���һ����д�ĵ�Ԫ��λ��(����ǰ���ַ���)
	int _maxCellLen;
	
	// ������
	typedef struct
	{
		int index;				// ÿһ�е���ʼ���
		int width;				// �п�� - һ�������е�Ԫ��ĳ���֮��(���ּ��)
		int y;					// ������� y ֵ
	}rowinfo_t;
	rowinfo_t* _rowInfo;		// ������
	int _sentryRowPos;			// �ڱ�λ��,�����һ�еĺ�һ��,����һЩ����,ȷ��������ͨ����ȡ��Ч�е���һ�еõ���Ϣ�Ա�����и�,�з�Χ. = ����
	int _rowIndexBufLen;		// ����������ĳ���
	int _widestRowIndex;		// �����е����
	bool _autoWrap;				// �Ƿ��Զ�����

	// ����ͼ����ڴ滺��
	HDC _hdc;
	HBITMAP _hbmp;
	POINT _windowSize;
	POINT _windowPos;
	HBRUSH _windowBkBrush;
	POINT _sel;					// ѡ������ [x, y)
	POINT _dirtyRange;			// ������ [x, y) �� y = -1 ʱ��ʾ���´ӵ�Ԫ x ����ʼ���굽�������Ӵ��ڵĽ�������.
	int _colSpacing;			// �ּ��
	int _rowSpacing;			// �м��
	int _alignMode;				// ���뷽ʽ

	MyCellCharacterFactory _charFactory;	// �ַ�����

	int reserve(int len);
	
	int setRowInfo(int row, int pos, int width, int y, unsigned int mask);	// ����������,���Զ�ά���������õ����ڴ�.
	int getRowInfo(int row, unsigned int mask) const;
	
	int drawc(int pos, const POINT& pt);
	int markDirtyRange(const POINT& rng);					// ��������� [x, y) �ַ�������ջ�ת��������ͼ���ڵ�һ������
	int getWidestRow(const POINT& rng) const;

	// ����������,����ԭ�е�����������. len < 0 ��ʾɾ��; len > 0 ��ʾ���.
	int updateRowIndex(int pos, int len);
	int updateWindow();
	
	int redraw();						// �ػ�����(�ӿ�)�ڵ�����
	int rebuildRowIndex();				// �ؽ�������

	// ��ֹ����
	MyVirtualImage(const MyVirtualImage&);
	MyVirtualImage& operator = (const MyVirtualImage&);
public:
	MyVirtualImage(int maxChars, const POINT& windowSz, bool autoWrap, HDC hdc);
	virtual ~MyVirtualImage();

	/*
	* ScreenBuffer ����
	*/

	// �Ƿ��Զ�����
	bool enableAutoWrap(bool autoWrap);

	// ��ȡ�и�, �м��, �ַ�ƽ�����,�ּ��
	int getRowSpacing() const;
	int getCharSpacing() const;

	/*
	* ����޸�ɾ��
	*/
	// remove(0, -1) ɾ��ȫ��
	int insert(int pos, MyCell** cells, int len);
	int remove(int pos, int len);
	MyCell* getCell(int pos);
	const MyCell* getCell(int pos) const;

	int insert(int pos, const wchar_t* ch, int len, const COLORREF& fg, const COLORREF& bk);
	int getText(int pos, int len, wchar_t* dest, int destLen) const;
	
	/*
	* ��ͼ��غ���
	*/
	// ��������, tabSize ��ʾ \t ռ�ü����ո�
	// OPAQUE / TRANSPARENT
	HFONT setFont(HFONT font, int rowSpacing, int colSpacing, int tabSize);
	int getCharAveWidth() const;
	int getCharHeight() const;
	int setBkMode(int m);
	HBRUSH setBkBrush(HBRUSH bkBrush);

	// ���������ı�����ʾʱ���ڼ�����ͼ��Ĵ�С(�û��Դ����ù���������Ϣ)
	POINT getImageSize() const;	
	
	// �����ӿڵĴ�С(����һ����Է�ʱ�Ĳ���,�����û�Ӧ�þ������ٵ��ô˺����Ĵ���,���粻Ҫ�ô��ڴ�С�����仯,���Ƿֶα仯)
	int setWindowSize(const POINT& sz);		
	int setWindowPos(const POINT& pos);
	int render(HDC hdc, const RECT& dest);
	
	// �ػ�ָ����Ԫ��
	int update(int pos, int len);	
	
	// ������ͼ������ ���,��������е��໥ת��
	// MyVirtualImage �������ַ�����һ��"����ͼ��"�����к���. ��������ʾ������ֻ���������ͼ���һ����.
	// pos ����Ч��Χ�� [0, size()]. ���� size() ����һ������ַ���λ��.

	// �����������(���ܰ�����Ч��Χ)����Ϊ��Ч�����[0, _curCellPos]
	int hitTest(const POINT& pt, unsigned int* htc = NULL) const;
	int hitTest(int row, int col, unsigned int* htc = NULL) const;

	POINT getPoint(int pos) const;
	int getRowCol(int pos, int* row, int* col) const;
	POINT getRowRange(int row) const;		// ���ĳ�е��ַ���ŷ�Χ(���еķ�Χ����[f, t))
	int getRowHeight(int row) const;
	int rows() const;
	int size() const;

	// ѡ��, getSel(): x = pos, y = len;
	int setSel(int pos, int len);
	int getSel(int* pos, int* len) const;
};