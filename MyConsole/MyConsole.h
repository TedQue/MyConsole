#pragma once
#include <string>
#include <vector>
#include "MyCell.h"

/*
* attach() ����Ϣ�Ĵ���ʽ: 
*/
// MODE_INTERCEPT ����: MyEdit ������ WM_ERASEBKGND,WM_SIZE,WM_VSCROLL,WM_HSCROLL,WM_MOUSEWHEEL,WM_MOUSEHWHEEL,WM_KEYDOWN,WM_PAINT
// MODE_FORWARD ת��: MyEdit ����������Ϣ֮����������ԭ���ڴ�����
#define MESSAGE_MODE_INTERCEPT 0
#define MESSAGE_MODE_FORWARD 1

/*
* ������ϢID�Լ�����Ľṹ�嶨��
* MyEdit / MyConsole ����Ҫ��Ϊ������ͨ�� SendMessage �ķ�ʽ����
*/
#define WM_EDIT_SETSEL (WM_USER + 100)
#define WM_EDIT_INSERT (WM_USER + 102)
#define WM_EDIT_REMOVE (WM_USER + 103)
#define WM_EDIT_GETSEL (WM_USER + 104)
#define WM_EDIT_GETTEXT (WM_USER + 105)
#define WM_EDIT_SHOWCARET (WM_USER + 106)
#define WM_EDIT_HIDECARET (WM_USER + 107)
#define WM_EDIT_SETCARETPOS (WM_USER + 108)
#define WM_EDIT_GETCARETPOS (WM_USER + 109)
#define WM_EDIT_MAKEVISIBLE (WM_USER + 110)
#define WM_EDIT_REPLACESEL (WM_USER + 111)

#define WM_CONSOLE_READ (WM_USER + 200)
#define WM_CONSOLE_WRITE (WM_USER + 201)
#define WM_CONSOLE_SWITCHMODE (WM_USER + 202)
#define WM_CONSOLE_CLEAR (WM_USER + 203)
#define WM_CONSOLE_EVENT (WM_USER + 204)
#define WM_CONSOLE_GETINPUT (WM_USER + 205)

// �ı���ص���Ϣʹ�õĽṹ WM_EDIT_INSERT / WM_CONSOLE_READ / WM_CONSOLE_WRITE
typedef struct
{
	int pos;
	MyCell* cell;
	const wchar_t* ch;
	int len;
	COLORREF fg;
	COLORREF bk;
}MYEDIT_CELLS;

// ��ȡ�ı�
typedef struct
{
	int pos;
	int len;
	MyCell* cell;
	wchar_t* ch;
	int chLen;
}MYEDIT_CELLSBUF;

// WM_EDIT_REMOVE ֱ��ʹ�� POINT �ṹ

// ѡ����صĽṹ ֱ��ʹ�� POINT �ṹ - WM_EDIT_SETSEL / WM_EDIT_GETSEL

// �û���Ҫ����Ŀ���̨�¼� - WM_CONSOLE_EVENT ��Ӧ������
#define CONSOLE_EVENT_NONE				0x0000
#define CONSOLE_EVENT_CHAR				0x0001	// ������һ���ַ�
#define CONSOLE_EVENT_RETRUN			0x0002	// �û�������һ������ �س�.
#define CONSOLE_EVENT_CTRL_RETRUN		0x0004	// �û�������һ������ CTRL + �س�.
#define CONSOLE_EVENT_ALT_RETRUN		0x0010	// �û�������һ������ ALT + �س�.

// ����̨ģʽ - WM_CONSOLE_SWITCHMODE
#define CONSOLEMODE_OUTPUT 0
#define	CONSOLEMODE_INPUT 1

/*
* ����Ԥ���峣��
*/
// IME ��ѡ�ַ�����, ��� IME �������ĳ��ȳ������ֵ��ʹ�ö�
#define IME_BUF_LEN 256
#define IME_CHAR_COLOR RGB(128,128,128)

// Ԥ���������ǰ��ɫ�ͱ���ɫ
#define CONSOLECOLOR_DEFAULT 0xFF000000

/*
* EDIT �ؼ�ʵ��
*/
class MyEdit
{
protected:
	LONG _wndProc;
	LONG _wndData;
	HWND _hWnd;

	// ����ͼ��
	MyVirtualImage* _vImage;

	// ��ʱ״ֵ̬ - ���������ۼ�ֵ,ÿ 120 ���¹���һ��,�������ҹ�������
	int _scrollLines;
	int _vDeltaAccr;
	int _hDeltaAccr;
	int _selFrom;
	bool _mouseMoved;

	// ��ǰ״̬������
	int _curCaretPos;		// ���λ��
	POINT _windowPos;
	POINT _windowSize;
	COLORREF _dftFgColor;
	COLORREF _dftBkColor;
	int _margin;			// �߿�
	HBRUSH _marginBrush;
	int _messageHandleMode;	// ��Ϣ����ģʽ
	bool _readOnly;
	HCURSOR _hIbeam;
	int _imeStartPos;
	int _imeLen;
	bool _dirty;			// ����
	
	// ���ߺ���
	int hitTest(const POINT& pt) const;			// point -> postion
	int hitTest(int row, int col) const;		// row, col -> postion
	int adjHitTest(int pos, int htc) const;		// �� MyVirtualImage::hitTest �����һ�������Է��� Edit ��ʹ��ϰ��
	int adjPos(int pos, bool forward) const;	// ��ǰ�������������ʹ�� \r\n ʼ����Ϊһ������
	void redraw();
	void updateScrollInfo();
	int scrollto(const POINT& pos);
	void markDirty();

	// ���ں���
	virtual LRESULT wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK StaticWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	MyEdit() = delete;
	MyEdit(const MyEdit&) = delete;
	MyEdit& operator = (const MyEdit&) = delete;

	// ʵ�ʲ����麯��(����������,����ˢ�´���)
	virtual int vmakeVisible(int pos);

	virtual int vsetSel(int pos, int len);
	virtual int vgetSel(int* pos, int* len) const;

	virtual int vshowCaret();
	virtual int vhideCaret();
	virtual int vsetCaretPos(int pos);
	virtual int vgetCaretPos() const;
	virtual int vrefreshCaret();		// ���ù�����ʾλ��

	virtual int vinsert(int pos, const wchar_t* ch, int len, COLORREF fg = CONSOLECOLOR_DEFAULT, COLORREF bk = CONSOLECOLOR_DEFAULT);
	virtual int vremove(int pos, int len);
	virtual int vgetText(int pos, int len, wchar_t* ch, int destLen) const;

	// �������Ϻ���(�Զ�ˢ��)
	virtual int vreplaceSel(const wchar_t* ch, int len, COLORREF fg = CONSOLECOLOR_DEFAULT, COLORREF bk = CONSOLECOLOR_DEFAULT);
public:
	MyEdit(int maxChars, bool autoWrap, HDC hdc);
	~MyEdit();

	int attach(HWND hwnd, int m);
	HWND detach();
	MyVirtualImage* getVImage() { return _vImage; }

	// ������Ļ��������ʹ���߿��ļ��
	int setMargin(int margin, COLORREF clr);
	int setDefaultColor(COLORREF fgColor, COLORREF bkColor);
	void enableReadOnly(bool r);
	int refresh();

	// ���� <-> ���
	int size() const;
	int lines() const;
	int getLineCol(int pos, int* ln, int* col) const;
	int getPosition(int ln, int col) const;
	int getLineLength(int ln) const;

	// ������
	int copy() const;
	int cut();
	int paste();

	/*
	* ͨ�� SendMessage() ʵ�ֵĲ���
	*/
	// ȷ���ɼ�
	int makeVisible(int pos);

	// ѡ��
	int setSel(int pos, int len);
	int getSel(int* pos, int* len) const;

	// ���ƹ��
	int showCaret();
	int hideCaret();
	int setCaretPos(int pos);
	int getCaretPos() const;

	// �༭�ı�
	int insert(int pos, const wchar_t* ch, int len, COLORREF fg = CONSOLECOLOR_DEFAULT, COLORREF bk = CONSOLECOLOR_DEFAULT);
	int remove(int pos, int len);
	int replaceSel(const wchar_t* ch, int len, COLORREF fg = CONSOLECOLOR_DEFAULT, COLORREF bk = CONSOLECOLOR_DEFAULT);
	int getText(int pos, int len, wchar_t* ch, int destLen) const;
};

/*
* ����̨�ؼ�ʵ��
*/
class MyConsole : private MyEdit
{
private:
	HWND _hEvWnd;		// �¼����մ���,���ȼ�: �û�ָ���Ĵ��� -> ������ -> ��ǰ����
	UINT _evMessageId;	// �¼� ID
	UINT _evMask;		// �¼�������
	
	int _messageHandleMode;	// ��Ϣ����ģʽ
	int _mode;				// ����ģʽ�������ģʽ
	wchar_t* _promptStr;	// ��������ʾ��
	int _inputStartPos;		// ����ģʽ����ʼλ��

	// ��������ʷ��¼
	int _maxHistory;
	int _curHistoryIndex;
	std::wstring _curInput;
	std::vector<std::wstring> _history;

	int getInputStartPos() const;
	void appendPrompt();
	int getWritableSel(int* pos, int* len) const; // ���ؿ�д���ֵ�ѡ��
	
	// ���ݿ���̨��Ҫ�����ع����ƺ��ı��޸ĺ���
	virtual int vshowCaret();
	virtual int vhideCaret();
	virtual int vsetCaretPos(int pos);		// ����ʵ��Ϊ���ֻ�������û����������߲�����(���ģʽ)
	virtual int vrefreshCaret();
	virtual int vreplaceSel(const wchar_t* ch, int len, COLORREF fg = CONSOLECOLOR_DEFAULT, COLORREF bk = CONSOLECOLOR_DEFAULT); // ����ʵ��Ϊֻ�滻��д����

	// ʵ�ֹ��ܵ��ڲ�����
	int doSwitchMode(int m, bool quiet = false);
	int doWrite(const wchar_t* ch, int len = -1, COLORREF fg = CONSOLECOLOR_DEFAULT, COLORREF bk = CONSOLECOLOR_DEFAULT);	// д����̨(���ģʽ��׷�ӵ�ĩβ;����ģʽ����ͬ replaceSel()
	int doRead(wchar_t* buf, int len) const;
	std::wstring doGetInput() const;
	int doClearInput();
	int doClear();

	// ���ں���
	virtual LRESULT wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	MyConsole() = delete;
	MyConsole(const MyConsole&) = delete;
	MyConsole& operator = (const MyConsole&) = delete;
public:
	MyConsole(int maxChars, bool autoWrap, HDC hdc);
	~MyConsole();

	int attach(HWND hwnd, int m);
	HWND detach();
	int setMargin(int margin, COLORREF clr);
	int setEventHandler(HWND hWnd, unsigned int msgid);	// ����̨�¼����մ���
	int setEventMask(unsigned int mask);
	int setPrompt(const wchar_t* str);			// ��������ʾ��,Ĭ��:[��ǰʱ��]$
	int enableHistory(int maxRecords);

	int write(const wchar_t* ch, int len = -1, COLORREF fg = CONSOLECOLOR_DEFAULT, COLORREF bk = CONSOLECOLOR_DEFAULT);	// д����̨(���ģʽ��׷�ӵ�ĩβ;����ģʽ����ͬ replaceSel()
	int read(wchar_t* buf, int len) const;		// ������̨ read(NULL, 0) �������뻺��ĳ���
	int switchMode(int m, bool quiet = false);	// �л�ģʽ 0: ���; �л������ģʽʱ,׷�ӻ��з�,���ع��. 1: ����. �л�������ģʽʱ,׷����������ʾ���͹��. ��ʼ��ʱ�������ģʽ.
	int mode() const;
	std::wstring getInput() const;
	int clearInput();
	int clear();								// ����
};

// ��Ϣѭ�� - ����Ϣ�����е�������Ϣ������󷵻�
// �����������ʱ����������Ϣ�޷������½�������Ӧ
int EatMessage();
void EatAllMessage();
