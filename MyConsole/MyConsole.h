#pragma once
#include <string>
#include <vector>
#include "MyCell.h"

/*
* attach() 对消息的处理方式: 
*/
// MODE_INTERCEPT 截留: MyEdit 会拦截 WM_ERASEBKGND,WM_SIZE,WM_VSCROLL,WM_HSCROLL,WM_MOUSEWHEEL,WM_MOUSEHWHEEL,WM_KEYDOWN,WM_PAINT
// MODE_FORWARD 转发: MyEdit 处理上述消息之后会继续调用原窗口处理函数
#define MESSAGE_MODE_INTERCEPT 0
#define MESSAGE_MODE_FORWARD 1

/*
* 功能消息ID以及所需的结构体定义
* MyEdit / MyConsole 的主要行为都可以通过 SendMessage 的方式控制
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

// 文本相关的消息使用的结构 WM_EDIT_INSERT / WM_CONSOLE_READ / WM_CONSOLE_WRITE
typedef struct
{
	int pos;
	MyCell* cell;
	const wchar_t* ch;
	int len;
	COLORREF fg;
	COLORREF bk;
}MYEDIT_CELLS;

// 获取文本
typedef struct
{
	int pos;
	int len;
	MyCell* cell;
	wchar_t* ch;
	int chLen;
}MYEDIT_CELLSBUF;

// WM_EDIT_REMOVE 直接使用 POINT 结构

// 选区相关的结构 直接使用 POINT 结构 - WM_EDIT_SETSEL / WM_EDIT_GETSEL

// 用户需要处理的控制台事件 - WM_CONSOLE_EVENT 对应的类型
#define CONSOLE_EVENT_NONE				0x0000
#define CONSOLE_EVENT_CHAR				0x0001	// 输入了一个字符
#define CONSOLE_EVENT_RETRUN			0x0002	// 用户发送了一个命令 回车.
#define CONSOLE_EVENT_CTRL_RETRUN		0x0004	// 用户发送了一个命令 CTRL + 回车.
#define CONSOLE_EVENT_ALT_RETRUN		0x0010	// 用户发送了一个命令 ALT + 回车.

// 控制台模式 - WM_CONSOLE_SWITCHMODE
#define CONSOLEMODE_OUTPUT 0
#define	CONSOLEMODE_INPUT 1

/*
* 其它预定义常数
*/
// IME 候选字符缓冲, 如果 IME 发送来的长度超过这个值将使用堆
#define IME_BUF_LEN 256
#define IME_CHAR_COLOR RGB(128,128,128)

// 预定义的字体前景色和背景色
#define CONSOLECOLOR_DEFAULT 0xFF000000

/*
* EDIT 控件实现
*/
class MyEdit
{
protected:
	LONG _wndProc;
	LONG _wndData;
	HWND _hWnd;

	// 虚拟图像
	MyVirtualImage* _vImage;

	// 临时状态值 - 鼠标滚动的累加值,每 120 向下滚动一行,或者向右滚动两列
	int _scrollLines;
	int _vDeltaAccr;
	int _hDeltaAccr;
	int _selFrom;
	bool _mouseMoved;

	// 当前状态和设置
	int _curCaretPos;		// 光标位置
	POINT _windowPos;
	POINT _windowSize;
	COLORREF _dftFgColor;
	COLORREF _dftBkColor;
	int _margin;			// 边框
	HBRUSH _marginBrush;
	int _messageHandleMode;	// 消息过滤模式
	bool _readOnly;
	HCURSOR _hIbeam;
	int _imeStartPos;
	int _imeLen;
	bool _dirty;			// 脏标记
	
	// 工具函数
	int hitTest(const POINT& pt) const;			// point -> postion
	int hitTest(int row, int col) const;		// row, col -> postion
	int adjHitTest(int pos, int htc) const;		// 对 MyVirtualImage::hitTest 结果作一个修正以符合 Edit 的使用习惯
	int adjPos(int pos, bool forward) const;	// 向前或者向后调整序号使得 \r\n 始终作为一个整体
	void redraw();
	void updateScrollInfo();
	int scrollto(const POINT& pos);
	void markDirty();

	// 窗口函数
	virtual LRESULT wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK StaticWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	MyEdit() = delete;
	MyEdit(const MyEdit&) = delete;
	MyEdit& operator = (const MyEdit&) = delete;

	// 实际操作虚函数(会设置脏标记,不会刷新窗口)
	virtual int vmakeVisible(int pos);

	virtual int vsetSel(int pos, int len);
	virtual int vgetSel(int* pos, int* len) const;

	virtual int vshowCaret();
	virtual int vhideCaret();
	virtual int vsetCaretPos(int pos);
	virtual int vgetCaretPos() const;
	virtual int vrefreshCaret();		// 设置光标的显示位置

	virtual int vinsert(int pos, const wchar_t* ch, int len, COLORREF fg = CONSOLECOLOR_DEFAULT, COLORREF bk = CONSOLECOLOR_DEFAULT);
	virtual int vremove(int pos, int len);
	virtual int vgetText(int pos, int len, wchar_t* ch, int destLen) const;

	// 方便的组合函数(自动刷新)
	virtual int vreplaceSel(const wchar_t* ch, int len, COLORREF fg = CONSOLECOLOR_DEFAULT, COLORREF bk = CONSOLECOLOR_DEFAULT);
public:
	MyEdit(int maxChars, bool autoWrap, HDC hdc);
	~MyEdit();

	int attach(HWND hwnd, int m);
	HWND detach();
	MyVirtualImage* getVImage() { return _vImage; }

	// 设置屏幕缓冲输出和窗体边框间的间距
	int setMargin(int margin, COLORREF clr);
	int setDefaultColor(COLORREF fgColor, COLORREF bkColor);
	void enableReadOnly(bool r);
	int refresh();

	// 行列 <-> 序号
	int size() const;
	int lines() const;
	int getLineCol(int pos, int* ln, int* col) const;
	int getPosition(int ln, int col) const;
	int getLineLength(int ln) const;

	// 剪贴板
	int copy() const;
	int cut();
	int paste();

	/*
	* 通过 SendMessage() 实现的操作
	*/
	// 确保可见
	int makeVisible(int pos);

	// 选区
	int setSel(int pos, int len);
	int getSel(int* pos, int* len) const;

	// 控制光标
	int showCaret();
	int hideCaret();
	int setCaretPos(int pos);
	int getCaretPos() const;

	// 编辑文本
	int insert(int pos, const wchar_t* ch, int len, COLORREF fg = CONSOLECOLOR_DEFAULT, COLORREF bk = CONSOLECOLOR_DEFAULT);
	int remove(int pos, int len);
	int replaceSel(const wchar_t* ch, int len, COLORREF fg = CONSOLECOLOR_DEFAULT, COLORREF bk = CONSOLECOLOR_DEFAULT);
	int getText(int pos, int len, wchar_t* ch, int destLen) const;
};

/*
* 控制台控件实现
*/
class MyConsole : private MyEdit
{
private:
	HWND _hEvWnd;		// 事件接收窗口,优先级: 用户指定的窗口 -> 父窗口 -> 当前窗口
	UINT _evMessageId;	// 事件 ID
	UINT _evMask;		// 事件屏蔽字
	
	int _messageHandleMode;	// 消息过滤模式
	int _mode;				// 输入模式或者输出模式
	wchar_t* _promptStr;	// 命令行提示符
	int _inputStartPos;		// 输入模式的起始位置

	// 命令行历史记录
	int _maxHistory;
	int _curHistoryIndex;
	std::wstring _curInput;
	std::vector<std::wstring> _history;

	int getInputStartPos() const;
	void appendPrompt();
	int getWritableSel(int* pos, int* len) const; // 返回可写部分的选区
	
	// 根据控制台的要求重载光标控制和文本修改函数
	virtual int vshowCaret();
	virtual int vhideCaret();
	virtual int vsetCaretPos(int pos);		// 重新实现为光标只出现在用户输入区或者不出现(输出模式)
	virtual int vrefreshCaret();
	virtual int vreplaceSel(const wchar_t* ch, int len, COLORREF fg = CONSOLECOLOR_DEFAULT, COLORREF bk = CONSOLECOLOR_DEFAULT); // 重新实现为只替换可写部分

	// 实现功能的内部函数
	int doSwitchMode(int m, bool quiet = false);
	int doWrite(const wchar_t* ch, int len = -1, COLORREF fg = CONSOLECOLOR_DEFAULT, COLORREF bk = CONSOLECOLOR_DEFAULT);	// 写控制台(输出模式是追加到末尾;输入模式功能同 replaceSel()
	int doRead(wchar_t* buf, int len) const;
	std::wstring doGetInput() const;
	int doClearInput();
	int doClear();

	// 窗口函数
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
	int setEventHandler(HWND hWnd, unsigned int msgid);	// 控制台事件接收窗口
	int setEventMask(unsigned int mask);
	int setPrompt(const wchar_t* str);			// 命令行提示符,默认:[当前时间]$
	int enableHistory(int maxRecords);

	int write(const wchar_t* ch, int len = -1, COLORREF fg = CONSOLECOLOR_DEFAULT, COLORREF bk = CONSOLECOLOR_DEFAULT);	// 写控制台(输出模式是追加到末尾;输入模式功能同 replaceSel()
	int read(wchar_t* buf, int len) const;		// 读控制台 read(NULL, 0) 返回输入缓冲的长度
	int switchMode(int m, bool quiet = false);	// 切换模式 0: 输出; 切换至输出模式时,追加换行符,隐藏光标. 1: 输入. 切换至输入模式时,追加命令行提示符和光标. 初始化时出于输出模式.
	int mode() const;
	std::wstring getInput() const;
	int clearInput();
	int clear();								// 清屏
};

// 消息循环 - 把消息队列中的所有消息处理掉后返回
// 用于连续输出时避免其它消息无法处理导致界面无响应
int EatMessage();
void EatAllMessage();
