#include "StdAfx.h"
#include <assert.h>
#include <Imm.h>
#include "MyConsole.h"
#include <time.h>



/*
* 常数定义
*/
#define LINES_PER_DELTA 3		// 鼠标滚动时,每个 DELTA 值对应的行数
#define CARET_HEIGHT 2			// 光标的高度



/*
* 消息循环,用于在连续输出时按需要调用,这样其它消息才有机会得到处理,界面卡顿的问题能缓解
*/
int EatMessage()
{
	MSG msg;
	if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		if(msg.message == WM_QUIT)
		{
			PostMessage(msg.hwnd, WM_QUIT, msg.wParam, msg.lParam);
			return -1;
		}
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		return 0;
	}
	return 1;
}

void EatAllMessage()
{
	while(0 == EatMessage())
	{
	}
}

void deflate_rect(RECT* rc, int n)
{
	rc->left += n;
	rc->right -= n;
	rc->top += n;
	rc->bottom -= n;
}

/*////////////////////////////////////////////////////////////////////////////////////////////////////
* MyEdit
*/////////////////////////////////////////////////////////////////////////////////////////////////////
LRESULT CALLBACK MyEdit::StaticWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	MyEdit* wnd = (MyEdit*)GetWindowLong(hWnd, GWL_USERDATA);
	return wnd->wndProc(hWnd, message, wParam, lParam);
}

MyEdit::MyEdit(int maxChars, bool autoWrap, HDC hdc)
{
	_wndProc = 0;
	_wndData = 0;
	_hWnd = NULL;

	_vImage = new MyVirtualImage(maxChars, {0, 0}, autoWrap, hdc);

	_margin = 0;
	_marginBrush = NULL;

	_vDeltaAccr = 0;
	_hDeltaAccr = 0;
	_scrollLines = 1;

	_windowPos.x = 0;
	_windowPos.y = 0;
	_windowSize.x = 0;
	_windowSize.y = 0;

	_dftFgColor = RGB(192,192,192);
	_dftBkColor = RGB(0,0,0);

	_curCaretPos = 0;
	_selFrom = -1;
	_readOnly = false;
	_dirty = true;

	_messageHandleMode = MESSAGE_MODE_INTERCEPT;

	_hIbeam = LoadCursor(NULL, IDC_IBEAM);
}

MyEdit::~MyEdit()
{
	detach();

	if(_vImage) delete _vImage;
	if(_marginBrush) DeleteObject(_marginBrush);
	if(_hIbeam) DestroyCursor(_hIbeam);
}

int MyEdit::size() const
{
	return _vImage->size();
}

int MyEdit::lines() const
{
	return _vImage->rows();
}

int MyEdit::getLineCol(int pos, int* ln, int* col) const
{
	if(pos < 0 || pos > size()) return 0;
	return _vImage->getRowCol(pos, ln, col);
}

int MyEdit::getPosition(int ln, int col) const
{
	return hitTest(ln, col);
}

int MyEdit::getLineLength(int ln) const
{
	POINT rng = {0, 0};
	if(ln > 0 && ln < lines()) rng = _vImage->getRowRange(ln);
	return rng.y - rng.x;
}

int MyEdit::setMargin(int mr, COLORREF clr)
{
	if(_marginBrush)
	{
		DeleteObject(_marginBrush);
		_marginBrush = NULL;
	}

	_margin = mr;
	_marginBrush = CreateSolidBrush(clr);

	markDirty();

	// 刷新窗口
	refresh();
	return 0;
}

// 只能影响之后的输出
int MyEdit::setDefaultColor(COLORREF fgColor, COLORREF bkColor)
{
	_dftFgColor = fgColor;
	_dftBkColor = bkColor;
	return 0;
}

// 当文本(虚拟图像)的大小发生变化时,根据新的大小更新滚动条的信息
void MyEdit::updateScrollInfo()
{
	HWND hWnd = _hWnd;
	RECT rcClient;
	GetClientRect(hWnd, &rcClient);
	if(rcClient.right - rcClient.left <= _margin || rcClient.bottom - rcClient.top <= _margin)
	{
		return;
	}
	deflate_rect(&rcClient, _margin);

	if(_vImage)
	{
		_windowSize.x = rcClient.right - rcClient.left;
		_windowSize.y = rcClient.bottom - rcClient.top;

		_vImage->setWindowSize(_windowSize);
		int lineHeight = _vImage->getCharHeight() + _vImage->getRowSpacing();

		// 设置滚动条的信息
		POINT lagetSz = _vImage->getImageSize();
		POINT windowPos = {0, 0};
					
		SCROLLINFO scrInfo = {0};
		scrInfo.cbSize = sizeof(SCROLLBARINFO);

		/*
		* 垂直滚动条. 垂直滚动条的最大值是 Visual Studio 方式的多一页少一行, 而不是 Edit 方式的最后一页最后一行
		*/
		scrInfo.fMask = SIF_RANGE | SIF_PAGE;
		scrInfo.nMin = 0;
		scrInfo.nMax = lagetSz.y + (rcClient.bottom - rcClient.top) - lineHeight - 1;
		scrInfo.nPage = rcClient.bottom - rcClient.top;
		SetScrollInfo(hWnd, SB_VERT, &scrInfo, TRUE);

		// 取回新值
		scrInfo.fMask = SIF_POS;
		GetScrollInfo(hWnd, SB_VERT, &scrInfo);
		windowPos.y = scrInfo.nPos;
		
		/*
		* 水平滚动条
		*/
		scrInfo.fMask = SIF_RANGE | SIF_PAGE;
		scrInfo.nMin = 0;
		scrInfo.nMax = lagetSz.x - 1;					// scrInfo.nMax = lagetSz.x 的话,水平滚动条会在不需要的时候出现.
		scrInfo.nPage = rcClient.right - rcClient.left;
		SetScrollInfo(hWnd, SB_HORZ, &scrInfo, TRUE);

		// 取回新值
		scrInfo.fMask = SIF_POS;
		GetScrollInfo(hWnd, SB_HORZ, &scrInfo);
		windowPos.x = scrInfo.nPos;

		// 重新设置窗口位置
		if(scrollto(windowPos))
		{
			// 由于改变窗口大小可能导致换行,所以需要刷新光标位置
			vrefreshCaret();
		}
	}
}

// 把虚拟图像滚到指定位置(不会自动刷新窗口)
int MyEdit::scrollto(const POINT& ppos)
{
	POINT pos = ppos;

	/*
	* 通过 GetScrollInfo() 函数确保 pos 设置的范围始终是有效的. 所以最终
	*/
	// 水平滚动条
	if(pos.x != _windowPos.x)
	{
		SCROLLINFO si = {0};
		si.cbSize = sizeof(SCROLLINFO);
		si.fMask = SIF_POS;
		si.nPos = pos.x;
		SetScrollInfo(_hWnd, SB_HORZ, &si, TRUE);
		GetScrollInfo(_hWnd, SB_HORZ, &si);
		pos.x = si.nPos;
	}

	// 垂直滚动条
	if(pos.y != _windowPos.y)
	{
		SCROLLINFO si = {0};
		si.cbSize = sizeof(SCROLLINFO);
		si.fMask = SIF_POS;
		si.nPos = pos.y;
		SetScrollInfo(_hWnd, SB_VERT, &si, TRUE);
		GetScrollInfo(_hWnd, SB_VERT, &si);
		pos.y = si.nPos;
	}

	// 滚动到指定位置
	if(_windowPos.x == pos.x && _windowPos.y == pos.y)
	{
		// 不需要滚动
		return 1;
	}
	else
	{
		_windowPos = pos;
		_vImage->setWindowPos(_windowPos);

		// 刷新光标位置
		vrefreshCaret();

		// 设置脏标记
		markDirty();
		return 0;
	}
}

// 用最少的滚动确保指定位置的字符可见
int MyEdit::makeVisible(int pos)
{
	return SendMessage(_hWnd, WM_EDIT_MAKEVISIBLE, 0, (LPARAM)pos);
}

// 返回1 表示已经可见,不需要刷新;否则需要刷新窗口
int MyEdit::vmakeVisible(int pos)
{
	int row = 0, col = 0;
	_vImage->getRowCol(pos, &row, &col);

	POINT pt = _vImage->getPoint(pos);
	POINT cellSz = {0,0};
	cellSz.y = row < _vImage->rows() ?  _vImage->getRowHeight(row) : 0;
	cellSz.x = _vImage->getCell(pos) ? _vImage->getCell(pos)->width() : 0;

	if((pt.x >= _windowPos.x && (pt.x + cellSz.x) <= (_windowPos.x + _windowSize.x)) 
		&& (pt.y >= _windowPos.y && (pt.y + cellSz.y) <= (_windowPos.y + _windowSize.y)))
	{
		// 已经可见
		return 1;
	}
	else
	{
		POINT ptNewPos;

		// 滚动到合适位置使 pos 字符可见
		// 原则: pt 在窗口上则往上滚动到刚刚能显示pt为止; 在左边往左滚动; 右边则往右滚动; 下边则往下滚动. 用最少的滚动距离显示出来即可.
		if(pt.x < _windowPos.x)
		{
			ptNewPos.x = pt.x;
		}
		else if(pt.x + cellSz.x > _windowPos.x + _windowSize.x)
		{
			ptNewPos.x = pt.x + cellSz.x - _windowSize.x;
		}
		else
		{
			// x 坐标在可视范围内
			ptNewPos.x = _windowPos.x;
		}
		
		if(pt.y < _windowPos.y)
		{
			ptNewPos.y = pt.y;
		}
		else if((pt.y + cellSz.y) > (_windowPos.y + _windowSize.y))
		{
			ptNewPos.y = pt.y + cellSz.y - _windowSize.y;
		}
		else
		{
			ptNewPos.y = _windowPos.y;
		}

		// 滚动到指定位置
		return scrollto(ptNewPos);
	}
}

// MyEdit 关联到一个窗口句柄,会占用 GWL_USERDATA 字段
// int m: MESSAGE_MODE_INTERCEPT / MESSAGE_MODE_FORWARD 分别对应拦截和转发消息处理模式.
int MyEdit::attach(HWND hWnd, int m)
{
	_messageHandleMode = m;

	_hWnd = hWnd;
	_wndData = GetWindowLong(hWnd, GWL_USERDATA);
	_wndProc = GetWindowLong(hWnd, GWL_WNDPROC);

	SetWindowLong(hWnd, GWL_USERDATA, (LONG)this);
	SetWindowLong(hWnd, GWL_WNDPROC, (LONG)StaticWndProc);

	return 0;
}

HWND MyEdit::detach()
{
	if(_hWnd == NULL) return NULL;	

	HWND hwnd = _hWnd;
	SetWindowLong(hwnd, GWL_USERDATA, _wndData);
	SetWindowLong(hwnd, GWL_WNDPROC, _wndProc);
	_hWnd = NULL;
	_wndData = 0;
	_wndProc = 0;
	_messageHandleMode = MESSAGE_MODE_INTERCEPT;

	return hwnd;
}

// 更新光标的位置
int MyEdit::vrefreshCaret()
{
	if(vgetCaretPos() >= 0 && vgetCaretPos() <= size())
	{
		POINT pt = _vImage->getPoint(vgetCaretPos());
		pt.x -= _windowPos.x;
		pt.y -= _windowPos.y;
		pt.x += _margin;
		pt.y += _margin;
		SetCaretPos(pt.x, pt.y);
	}
	return 0;
}

// 创建并显示光标
int MyEdit::showCaret()
{
	return SendMessage(_hWnd, WM_EDIT_SHOWCARET, 0, 0);
}

int MyEdit::vshowCaret()
{
	// 创建光标
	if(!CreateCaret(_hWnd, NULL, 1, _vImage->getCharHeight()))
	{
		assert(0);
	}
	else
	{
		ShowCaret(_hWnd);
	}

	return 0;
}

int MyEdit::hideCaret()
{
	return SendMessage(_hWnd, WM_EDIT_HIDECARET, 0, 0);
}

int MyEdit::vhideCaret()
{
	DestroyCaret();
	return 0;
}

// 设置光标的位置
int MyEdit::setCaretPos(int pos)
{
	return SendMessage(_hWnd, WM_EDIT_SETCARETPOS, 0, pos);
}

int MyEdit::vsetCaretPos(int pos)
{
	// 修正有效范围,光标的有效范围是 [0, size()]
	if(pos < 0) pos = 0;
	else if(pos > size()) pos = size();

	if(pos == _curCaretPos)
	{
	}
	else
	{
		_curCaretPos = pos;
		vrefreshCaret();
	}

	return _curCaretPos;
}

// 获取光标位置(不需要通过发送消息)
int MyEdit::getCaretPos() const
{
	return vgetCaretPos();
}

int MyEdit::vgetCaretPos() const
{
	return _curCaretPos;
}

// 向前或者向后调整位置使得 \r\n 始终是一个整体
int MyEdit::adjPos(int pos, bool forward) const
{
	wchar_t chn = 0, chr = 0;
	if(forward)
	{
		if(pos >= 0 && (1 == _vImage->getText(pos, 1, &chr, 1)) && (1 == _vImage->getText(pos + 1, 1, &chn, 1)))
		{
			if(L'\r' == chr && L'\n' == chn)
			{
				pos++;
			}
		}
	}
	else
	{
		if(pos > 0 && (1 == _vImage->getText(pos - 1, 1, &chr, 1)) && (1 == _vImage->getText(pos, 1, &chn, 1)))
		{
			if(L'\r' == chr && L'\n' == chn)
			{
				pos--;
			}
		}
	}
	return pos;
}

int MyEdit::adjHitTest(int pos, int htc) const
{
	do
	{
		// 行溢出 
		if(htc & MYHTC_ROWOVF)
		{
			assert(pos == size());
			break;
		}

		// 列溢出: 返回值已经指向该行的最后一个字符
		if(htc & MYHTC_COLOVF)
		{
			// 如果点击范围属于一个换行符的位置,则把光标指向换行符前面以便把新字符插入到换行符之前
			const MyCell* c = _vImage->getCell(pos);
			if(c && c->wrap())
			{
				// 如果光标指向 \n 并且之前有一个 \r 则调整光标位置. \r\n 作为一个整体,中间不能插入其它字符.
				pos = adjPos(pos, false);
			}
			else
			{
				// 否则光标移到下一个字符(只有在最后一行的特例才会发生)
				pos++;
				//assert(pos == size());
			}
			break;
		}

		// 鼠标点在单元格(字符)的右半边算下一个字符
		if(htc & MYHTC_RIGHT)
		{
			pos++;
			break;
		}
	}while(0);
	return pos;
}

int MyEdit::hitTest(const POINT& pt) const
{
	// 根据点 hitTest 测算光标的位置
	unsigned int htc = 0;
	int pos = _vImage->hitTest(pt, &htc);
	return adjHitTest(pos, htc);
}

int MyEdit::hitTest(int row, int col) const
{
	unsigned int htc = 0;
	int pos = _vImage->hitTest(row, col, &htc);
	return adjHitTest(pos, htc);
}

int MyEdit::setSel(int pos, int len)
{
	POINT sel = {pos, len};
	return SendMessage(_hWnd, WM_EDIT_SETSEL, 0, (LPARAM)&sel);
}

int MyEdit::vsetSel(int pos, int len)
{
	// 过滤选区没有变化的情况,避免无用刷新
	int curSelPos, curSelLen;
	vgetSel(&curSelPos, &curSelLen);
	if(curSelPos == pos && curSelLen == len) return 1;

	// 修正参数,确保有效性
	if(len < 0) len = size();
	if(pos < 0) pos = 0;
	if(pos + len > size()) len = size() - pos;

	// 为 \r\n 而调整
	if(len > 0)
	{
		int posTo = pos + len - 1;
		pos = adjPos(pos, false);
		posTo = adjPos(posTo, true);
		len = posTo - pos + 1;
	}

	// 设置新选区
	_vImage->setSel(pos, len);
	markDirty();
	return 0;
}

// 获取全部选区或者可写的选区
int MyEdit::getSel(int* pos, int* len) const
{
	POINT sel;
	SendMessage(_hWnd, WM_EDIT_GETSEL, 0, (LPARAM)&sel);

	*pos = sel.x;
	*len = sel.y;
	return 0;
}

int MyEdit::vgetSel(int* pos, int* len) const
{
	return _vImage->getSel(pos, len);
}

void MyEdit::enableReadOnly(bool r)
{
	_readOnly = r;
}

int MyEdit::getText(int pos, int len, wchar_t* ch, int destLen) const
{
	MYEDIT_CELLSBUF buf;
	buf.pos = pos;
	buf.len = len;
	buf.ch = ch;
	buf.chLen = destLen;
	SendMessage(_hWnd, WM_EDIT_GETTEXT, 0, (LPARAM)&buf);

	return buf.len;
}

int MyEdit::vgetText(int pos, int len, wchar_t* ch, int destLen) const
{
	return _vImage->getText(pos, len, ch, destLen);
}

// 在指定位置前插入文本,完成后需要更新选区和光标的位置
int MyEdit::insert(int pos, const wchar_t* ch, int len, COLORREF fg, COLORREF bk)
{
	MYEDIT_CELLS cellInfo;
	cellInfo.pos = pos;
	cellInfo.len = len;
	cellInfo.ch = ch;
	cellInfo.fg = fg;
	cellInfo.bk = bk;
	SendMessage(_hWnd, WM_EDIT_INSERT, 0, (LPARAM)&cellInfo);

	return cellInfo.len;
}

int MyEdit::vinsert(int pos, const wchar_t* ch, int len, COLORREF fg, COLORREF bk)
{
	if(_readOnly) return 0;
	if(CONSOLECOLOR_DEFAULT == fg) fg = _dftFgColor;
	if(CONSOLECOLOR_DEFAULT == bk) bk = _dftBkColor;

	// 修正位置
	if(pos < 0) pos = 0;
	if(pos > size()) pos = size();

	// 插入文本
	len = _vImage->insert(pos, ch, len, fg, bk);

	// 更新光标位置
	if(pos <= vgetCaretPos())
	{
		vsetCaretPos(vgetCaretPos() + len);
	}
	else
	{
		// 光标保持不变
	}

	// 可以计算得出是否形成脏区域
	markDirty();
	return len;
}

int MyEdit::replaceSel(const wchar_t* ch, int len, COLORREF fg, COLORREF bk)
{
	MYEDIT_CELLS info;
	info.ch = ch;
	info.len = len;
	info.cell = NULL;
	info.fg = fg;
	info.bk = bk;
	SendMessage(_hWnd, WM_EDIT_REPLACESEL, 0, (LPARAM)&info);
	return info.len;
}

int MyEdit::vreplaceSel(const wchar_t* ch, int len, COLORREF fg, COLORREF bk)
{
	// 删除选区内的文本,如果有选区的话
	int pos = 0;
	int selPos, selLen;
	vgetSel(&selPos, &selLen);
	if(selLen > 0)
	{
		vremove(selPos, selLen);
		pos = selPos;
	}
	else
	{
		pos = vgetCaretPos();
	}

	// 在原选区或者光标位置插入新文本
	if(len > 0)
	{
		len = vinsert(pos, ch, len, fg, bk);
	}

	// 先更新滚动条信息
	updateScrollInfo();

	// 确保光标可见
	if(!vmakeVisible(vgetCaretPos()))
	{
	}

	// 刷新窗口
	redraw();

	return len;
}

int MyEdit::remove(int pos, int len)
{
	POINT info = {pos, len};
	SendMessage(_hWnd, WM_EDIT_REMOVE, 0, (LPARAM)&info);
	return info.x;
}

int MyEdit::vremove(int pos, int len)
{
	if(_readOnly) return 0;

	// 修正参数,确保有效性
	if(pos < 0) pos = 0;
	if(pos > size()) pos = size();
	if(len < 0) len = size();
	if(pos + len > size()) len = size() - pos;

	// 删除文本
	len = _vImage->remove(pos, len);

	// 更新光标位置
	if(vgetCaretPos() <= pos)
	{
		// 光标在被删除的文本之前,不需要移动
	}
	else if(vgetCaretPos() > pos && vgetCaretPos() <= pos + len)
	{
		vsetCaretPos(pos);
	}
	else
	{
		vsetCaretPos(vgetCaretPos() - len);
	}

	// 计算是否形成脏区域
	markDirty();
	return len;
}

void MyEdit::markDirty()
{
	_dirty = true;
}

void MyEdit::redraw()
{
	if(_dirty)
	{
		_dirty = false;
		InvalidateRect(_hWnd, NULL, TRUE);
		UpdateWindow(_hWnd);
	}
}

int MyEdit::refresh()
{
	// 更新滚动条信息
	updateScrollInfo();
	redraw();

	return 0;
}

// 不管处于输入模式还是输出模式,允许选中任何字符, CTRL + C 则把这些字符写入剪贴板
int MyEdit::copy() const
{
	int pos, len;
	_vImage->getSel(&pos, &len);
	if(OpenClipboard(NULL))
	{
		EmptyClipboard(); 
		if(len > 0)
		{
			HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, (len + 1) * sizeof(WCHAR));
			WCHAR* pSelText = (WCHAR*)GlobalLock(hMem);
			if(pSelText)
			{
				vgetText(pos, len, pSelText, len);
				pSelText[len] = 0;
				GlobalUnlock(hMem);

				SetClipboardData(CF_UNICODETEXT, hMem);
			}
		}
		CloseClipboard();
	}
	return 0;
}

// 读部分同 copy(), 删除部分只有处于输入状态中用户输入的字符可以删除
int MyEdit::cut()
{
	copy();
	vreplaceSel(NULL, 0, 0, 0);
	return 0;
}

int MyEdit::paste()
{
	if(OpenClipboard(NULL))
	{
		if(IsClipboardFormatAvailable(CF_UNICODETEXT))
		{
			HGLOBAL hMem = GetClipboardData(CF_UNICODETEXT);
			WCHAR* pText = (WCHAR*)GlobalLock(hMem);

			vreplaceSel(pText, wcslen(pText), _dftFgColor, _dftBkColor);

			GlobalUnlock(hMem);
		}
		CloseClipboard();
	}
	return 0;
}

/*
* 截获显示,滚动相关的消息
*/
LRESULT MyEdit::wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	assert(hWnd == _hWnd);
	switch (message)
	{
		case WM_ERASEBKGND:
			{
				// 只画 setMargin() 指定的边框.
				if(_margin > 0)
				{
					HDC hdc = (HDC)wParam;
					RECT rcClient;
					GetClientRect(hWnd, &rcClient);

					RECT rcMargin = {0, 0, rcClient.right, _margin};
					FillRect(hdc, &rcMargin, _marginBrush);

					rcMargin = {rcClient.right - _margin, 0, rcClient.right, rcClient.bottom};
					FillRect(hdc, &rcMargin, _marginBrush);

					rcMargin = {0, rcClient.bottom - _margin, rcClient.right, rcClient.bottom};
					FillRect(hdc, &rcMargin, _marginBrush);

					rcMargin = {0, 0, _margin, rcClient.bottom};
					FillRect(hdc, &rcMargin, _marginBrush);
				}
			}
			break;
		case WM_SIZE:
			{
				if(wParam == SIZE_MAXIMIZED || wParam == SIZE_RESTORED)
				{
					// MSDN: WM_SIZE 消息可能会触发 WM_PAIN 也可能不会, 不需要调用 refresh() 重复刷新,只需要更新滚动条设置即可
					updateScrollInfo();
				}
			}
			break;
		case WM_SETCURSOR:
			{
				// WM_SETCURSOR 的消息特殊处理
				if(LOWORD(lParam) == HTCLIENT)
				{
					SetCursor(_hIbeam);
					return TRUE;
				}
			}
			break;
		case WM_VSCROLL:
		{
			if(_vImage)
			{
				// 获取或者计算新的位置
				SCROLLINFO si = {0};
				si.cbSize = sizeof(SCROLLINFO);
				si.fMask = SIF_ALL;
				GetScrollInfo(hWnd, SB_VERT, &si);
				
				// assert(si.nPos == _windowPos.y);
				int vPos = si.nPos;
				int lineHeight = _vImage->getCharHeight() + _vImage->getRowSpacing();

				switch(LOWORD(wParam))
				{
					// 作为窗口的滚动条(滚动条也可以是一个独立的窗口)不会收到 SB_TOP, SB_BOTTOM, SB_LEFT, SB_RIGHT,除非主动发送滚动到底部的消息
					case SB_TOP:
					{
						vPos = si.nMin;
					}
					break;
					case SB_BOTTOM:
					{
						vPos = si.nMax;
					}
					break;
					case SB_LINEUP:
					{
						vPos -= (lineHeight * _scrollLines);
					}
					break;
					case SB_LINEDOWN:
					{
						vPos += (lineHeight * _scrollLines);
					}
					break;
					case SB_PAGEUP:
					{
						vPos -= si.nPage;
					}
					break;
					case SB_PAGEDOWN:
					{
						vPos += si.nPage;
					}
					break;
					case SB_THUMBTRACK:
					{
						vPos = si.nTrackPos;
					}
					break;
					case SB_THUMBPOSITION:
					{
						vPos = si.nTrackPos;
					}
					break;
					default:
						break;
				}

				POINT newPos = {_windowPos.x, vPos};
				scrollto(newPos);
				redraw();
			}
		}
		break;
		case WM_HSCROLL:
		{
			if(_vImage)
			{
				SCROLLINFO si = {0};
				si.cbSize = sizeof(SCROLLINFO);
				si.fMask = SIF_ALL;
				GetScrollInfo(hWnd, SB_HORZ, &si);

				assert(si.nPos == _windowPos.x);
				int hPos = si.nPos;
				int charAveWidth = _vImage->getCharAveWidth();

				switch(LOWORD(wParam))
				{
					case SB_LINELEFT:
					{
						hPos -= (charAveWidth * _scrollLines);
					}
					break;
					case SB_LINERIGHT:
					{
						hPos += (charAveWidth * _scrollLines);
					}
					break;
					case SB_PAGELEFT:
					{
						hPos -= si.nPage;
					}
					break;
					case SB_PAGERIGHT:
					{
						hPos += si.nPage;
					}
					break;
					case SB_THUMBTRACK:
					{
						hPos = si.nTrackPos;
					}
					break;
					case SB_THUMBPOSITION:
					{
						hPos = si.nTrackPos;
					}
					break;
					default:
						break;
				}

				POINT newPos = {hPos, _windowPos.y};
				scrollto(newPos);
				redraw();
			}
		}
		break;
		case WM_MOUSEWHEEL:
		{
			// 滚轮消息转化为 滚动条消息
			if(_vImage)
			{
				// WHEEL_DELTA 120 这个数字不是我定的,有问题找微软
				_vDeltaAccr += GET_WHEEL_DELTA_WPARAM(wParam);
				if(_vDeltaAccr >= WHEEL_DELTA)
				{
					// 每个DELTA滚动3行
					_scrollLines = LINES_PER_DELTA * (_vDeltaAccr / WHEEL_DELTA);
					SendMessage(hWnd, WM_VSCROLL, SB_LINEUP, 0);
					_scrollLines = 1;
					_vDeltaAccr %= WHEEL_DELTA;
				}
				else if(_vDeltaAccr <= -WHEEL_DELTA)
				{
					_scrollLines = LINES_PER_DELTA * (_vDeltaAccr / -WHEEL_DELTA);
					SendMessage(hWnd, WM_VSCROLL, SB_LINEDOWN, 0);
					_scrollLines = 1;
					_vDeltaAccr %= WHEEL_DELTA;
				}
				else
				{
				}
			}
		}
		break;
		case WM_MOUSEHWHEEL:
		{
			if(_vImage)
			{
				_hDeltaAccr += GET_WHEEL_DELTA_WPARAM(wParam);
				if(_hDeltaAccr >= WHEEL_DELTA)
				{
					_scrollLines = LINES_PER_DELTA * (_hDeltaAccr / WHEEL_DELTA);
					SendMessage(hWnd, WM_HSCROLL, SB_LINELEFT, 0);
					_scrollLines = 1;
					_hDeltaAccr %= WHEEL_DELTA;
				}
				else if(_hDeltaAccr <= -WHEEL_DELTA)
				{
					_scrollLines = LINES_PER_DELTA * (_hDeltaAccr / -WHEEL_DELTA);
					SendMessage(hWnd, WM_VSCROLL, SB_LINERIGHT, 0);
					_scrollLines = 1;
					_hDeltaAccr %= WHEEL_DELTA;
				}
				else
				{
				}
			}
		}
		break;
		case WM_KEYDOWN:
		{
			// 只拦截滚动相关的按键,其它按键事件还是有原来的窗口处理函数处理
			switch (wParam)
			{
				case VK_HOME:
				{
					SendMessage(hWnd, WM_VSCROLL, SB_TOP, 0);
				}
				break;
				case VK_END:
				{
					SendMessage(hWnd, WM_VSCROLL, SB_BOTTOM, 0);
				}
				break;
				case VK_PRIOR:
				{
					SendMessage(hWnd, WM_VSCROLL, SB_PAGEUP, 0);
				}
				break;
				case VK_NEXT:
				{
					SendMessage(hWnd, WM_VSCROLL, SB_PAGEDOWN, 0);
				}
				break;
				case VK_UP:
				{
					if(_readOnly)
					{
						SendMessage(hWnd, WM_VSCROLL, SB_LINEUP, 0);
					}
					else
					{
						// 首先清除选区
						vsetSel(0, 0);

						// 光标上移一行
						int l = 0, c = 0;
						getLineCol(_curCaretPos, &l, &c);
						if(l > 0)
						{
							vsetCaretPos(hitTest(l - 1, c));
							vmakeVisible(vgetCaretPos());
						}

						// 刷新脏区域
						redraw();
					}
				}
				break;
				case VK_DOWN:
				{
					if(_readOnly)
					{
						SendMessage(hWnd, WM_VSCROLL, SB_LINEDOWN, 0);
					}
					else
					{
						vsetSel(0, 0);

						int l = 0, c = 0;
						getLineCol(_curCaretPos, &l, &c);
						if(l < lines() - 1)
						{
							vsetCaretPos(hitTest(l + 1, c));
							vmakeVisible(vgetCaretPos());
						}

						redraw();
					}
				}
				break;
				case VK_LEFT:
				{
					if(_readOnly)
					{
						SendMessage(hWnd, WM_HSCROLL, SB_PAGEUP, 0);
					}
					else
					{
						int newCaretPos = 0;
						int pos, len;
						getSel(&pos, &len);
						if(len > 0)
						{
							newCaretPos = pos - 1;
						}
						else
						{
							newCaretPos = _curCaretPos - 1;
						}

						vsetSel(0, 0);
						vsetCaretPos(adjPos(newCaretPos, false));
						vmakeVisible(vgetCaretPos());
						redraw();
					}
				}
				break;
				case VK_RIGHT:
				{
					if(_readOnly)
					{
						SendMessage(hWnd, WM_HSCROLL, SB_PAGEDOWN, 0);
					}
					else
					{
						int newCaretPos = 0;
						int pos, len;
						getSel(&pos, &len);
						if(len > 0)
						{
							newCaretPos = pos + len + 1;
						}
						else
						{
							newCaretPos = adjPos(_curCaretPos, true) + 1;
						}

						vsetSel(0, 0);
						vsetCaretPos(newCaretPos);
						vmakeVisible(vgetCaretPos());
						redraw();
					}
				}
				break;
				case 0x41: // CTRL + A
				{
					if(GetKeyState(VK_CONTROL) < 0)
					{
						vsetSel(0, -1);
						redraw();
					}
				}
				break;
				case 0x43: // CTRL + C
				{
					if(GetKeyState(VK_CONTROL) < 0)
					{
						copy();
					}
				}
				break;
				case 0x56: // CTRL + V
				{
					if(GetKeyState(VK_CONTROL) < 0)
					{
						paste();
					}
				}
				break;
				case 0x58: // CTRL + X
				{
					if(GetKeyState(VK_CONTROL) < 0)
					{
						cut();
					}
				}
				break;
				case VK_DELETE:
				{
					int pos, len;
					vgetSel(&pos, &len);
					if(len > 0)
					{
						vreplaceSel(NULL, 0, 0, 0);
					}
					else if(vgetCaretPos() >= size())
					{
					}
					else
					{
						vsetSel(_curCaretPos, 1);
						vreplaceSel(NULL, 0, 0, 0);
					}
				}
				break;
				//case VK_PROCESSKEY:
				//{
				//	// 输入法预选
				//}
				//break;
				default:
					return ((WNDPROC)_wndProc)(hWnd, message, wParam, lParam);
			}
		}
		break;
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			if(_vImage)
			{
				RECT rcClient;
				GetClientRect(hWnd, &rcClient);		
				deflate_rect(&rcClient, _margin);

				_vImage->render(hdc, rcClient);
			}
			EndPaint(hWnd, &ps);
		}
		break;
		case WM_LBUTTONDOWN:
		{
			// 设置键盘焦点
			if(hWnd != GetFocus())
			{
				SetFocus(hWnd);
			}

			if(_vImage)
			{
				POINT pt = {LOWORD(lParam), HIWORD(lParam)};
				pt.x += _windowPos.x;
				pt.y += _windowPos.y;
				
				// 根据点 hitTest 测算光标的位置
				_selFrom = hitTest(pt);
				 vsetCaretPos(_selFrom);
				 _mouseMoved = false;
			}
		}	
		break;
		case WM_LBUTTONUP:
		{
			if(_vImage)
			{
				POINT pt = {LOWORD(lParam), HIWORD(lParam)};
				pt.x += _windowPos.x;
				pt.y += _windowPos.y;

				int pos = hitTest(pt);
				if(_mouseMoved)
				{
					// 在左键按下,鼠标移动的过程中已经设置了选区,设置光标的位置
					int selPos = 0, selLen = 0;
					getSel(&selPos, &selLen);
					if(selPos < _selFrom)
					{
						vsetCaretPos(pos);
					}
					else
					{
						vsetCaretPos(selPos + selLen);
					}
				}
				else
				{
					// 把原选区删除
					vsetSel(0, 0);
					redraw();
				}

				_selFrom = -1;
			}
		}
		break;
		case WM_MOUSEMOVE:
		{
			if((MK_LBUTTON & wParam) && _selFrom >= 0)
			{
				_mouseMoved = true;
				if(_vImage)
				{
					POINT pt = {LOWORD(lParam), HIWORD(lParam)};
					pt.x += _windowPos.x;
					pt.y += _windowPos.y;
					int pos = hitTest(pt);
					if(pos == _selFrom)
					{
					}
					else if(pos > _selFrom)
					{
						vsetSel(_selFrom, pos - _selFrom);
						redraw();
					}
					else
					{
						vsetSel(pos, _selFrom - pos);
						redraw();
					}
				}
			}
		}
		break;
		case WM_SETFOCUS:
		{
			vshowCaret();
			vrefreshCaret();
		}
		break;
		case WM_KILLFOCUS:
		{
			vhideCaret();
		}
		break;
		case WM_IME_STARTCOMPOSITION:
		{
			COMPOSITIONFORM ci;
			ci.dwStyle = CFS_POINT;
			ci.ptCurrentPos = _vImage->getPoint(_curCaretPos);
			ci.ptCurrentPos.x -= _windowPos.x;
			ci.ptCurrentPos.y -= _windowPos.y;
			SendMessage(ImmGetDefaultIMEWnd(hWnd), WM_IME_CONTROL, IMC_SETCOMPOSITIONWINDOW, (LPARAM)&ci);

			_imeStartPos = _curCaretPos;
			_imeLen = 0;
		}
		break;
		case WM_IME_COMPOSITION:
		{
			WCHAR sbuf[IME_BUF_LEN] = {0};
			HIMC hIMC = ImmGetContext(hWnd);

			if(lParam & GCS_RESULTSTR)
			{
				// 输入输入法的输入结果
				DWORD dwSize = ImmGetCompositionString(hIMC, GCS_RESULTSTR, NULL, 0);
				dwSize += sizeof(WCHAR);
				WCHAR* lpstr = sbuf;
				if(dwSize > IME_BUF_LEN * sizeof(WCHAR))
				{
					lpstr = new WCHAR[dwSize];
					memset(lpstr, 0, sizeof(WCHAR) * dwSize);
					assert(0);
				}

				// Get the result strings that is generated by IME into lpstr. 
				ImmGetCompositionString(hIMC, GCS_RESULTSTR, lpstr, dwSize);

				// add this string into text buffer of application 
				vsetSel(_imeStartPos, _imeLen);
				vreplaceSel(lpstr, wcslen(lpstr), _dftFgColor, _dftBkColor);

				if(lpstr != sbuf)
				{
					delete []lpstr;
				}
			}
			else
			{
				if (lParam & GCS_COMPSTR) 
				{
					// 显示输入法的输入字符
					DWORD dwSize = ImmGetCompositionString(hIMC, GCS_COMPSTR, NULL, 0);
					dwSize += sizeof(WCHAR);
					WCHAR* lpstr = sbuf;
					if(dwSize > IME_BUF_LEN * sizeof(WCHAR))
					{
						lpstr = new WCHAR[dwSize];
						memset(lpstr, 0, sizeof(WCHAR) * dwSize);
						assert(0);
					}

					// Get the result strings that is generated by IME into lpstr. 
					LONG l = ImmGetCompositionString(hIMC, GCS_COMPSTR, lpstr, dwSize);
					
					// add this string into text buffer of application 
					vsetSel(_imeStartPos, _imeLen);
					vreplaceSel(lpstr, wcslen(lpstr), IME_CHAR_COLOR, _dftBkColor);
					_imeLen = (int)l / sizeof(WCHAR);

					if(lpstr != sbuf)
					{
						delete []lpstr;
					}
				}

				if (lParam & GCS_CURSORPOS)
				{
					LONG lpos = ImmGetCompositionString(hIMC, GCS_CURSORPOS, NULL, 0);
					lpos &= 0xFFFF;
					vsetCaretPos(_imeStartPos + lpos);
				}
			}

			ImmReleaseContext(hWnd, hIMC);
		}
		break;
		case WM_IME_ENDCOMPOSITION:
		{
			_imeStartPos = 0;
		}
		break;
		case WM_IME_CHAR:
		case WM_CHAR:
		{
			if(_readOnly) break;
			
			// 事件被目标窗口忽略,继续处理
			if(_vImage)
			{
				switch(wParam)
				{
					case VK_RETURN:
					{
						vreplaceSel(L"\r\n", 2, _dftFgColor, _dftBkColor);
					}
					break;
					case 0x01:	// CTRL + A
					case 0x03:  // CTRL + C
					case 0x16:	// CTRL + V
					case 0x18:	// CTRL + X
					{
					}
					break;
					case VK_BACK:
					{
						if(size() > 0)
						{
							int pos, len;
							vgetSel(&pos, &len);
							if(len > 0)
							{
							}
							else if(_curCaretPos > 0)
							{
								// 删除光标前的一个字符
								vsetSel(_curCaretPos - 1, 1); 
							}
							vreplaceSel(NULL, 0, 0, 0);
						}
					}
					break;
					default:
					{
						vreplaceSel((const wchar_t*)&wParam, 1, _dftFgColor, _dftBkColor);
					}
				}
			}
		}
		break;
		case WM_EDIT_SHOWCARET:
		{
			vshowCaret();
		}
		break;
		case WM_EDIT_HIDECARET:
		{
			vhideCaret();
		}
		break;
		case WM_EDIT_SETCARETPOS:
		{
			vsetCaretPos((int)lParam);
			vmakeVisible(vgetCaretPos());
			redraw();
		}
		break;
		case WM_EDIT_GETCARETPOS:
		{
			int* pos = (int*)lParam;
			*pos = vgetCaretPos();
		}
		break;
		case WM_EDIT_MAKEVISIBLE:
		{
			vmakeVisible((int)lParam);
			redraw();
		}
		break;
		case WM_EDIT_SETSEL:
		{
			POINT* sel = (POINT*)lParam;
			vsetSel(sel->x, sel->y);
			redraw();
		}
		break;
		case WM_EDIT_GETSEL:
		{
			POINT* sel = (POINT*)lParam;
			vgetSel((int*)&sel->x, (int*)&sel->y);
		}
		break;
		case WM_EDIT_GETTEXT:
		{
			MYEDIT_CELLSBUF *buf = (MYEDIT_CELLSBUF*)lParam;
			buf->len = vgetText(buf->pos, buf->len, buf->ch, buf->chLen);
		}
		break;
		case WM_EDIT_INSERT:
		{
			MYEDIT_CELLS *cellInfo = (MYEDIT_CELLS*)lParam;
			cellInfo->len = vinsert(cellInfo->pos, cellInfo->ch, cellInfo->len, cellInfo->fg, cellInfo->bk);
			refresh();
		}
		break;
		case WM_EDIT_REMOVE:
		{
			POINT *info = (POINT*)lParam;
			info->x = vremove(info->x, info->y);
			refresh();
		}
		break;
		case WM_EDIT_REPLACESEL:
		{
			MYEDIT_CELLS *info = (MYEDIT_CELLS*)lParam;
			info->len = vreplaceSel(info->ch, info->len, info->fg, info->bk);
		}
		break;
		default:
		{
			// 其它消息调用原来的处理函数
			return ((WNDPROC)_wndProc)(hWnd, message, wParam, lParam);
		}
	}

	if(MESSAGE_MODE_INTERCEPT == _messageHandleMode)
	{
		// 截留模式下, WM_SETCURSOR 特殊处理,调用默认处理函数来设置默认指针样式
		if(message == WM_SETCURSOR) return ((WNDPROC)_wndProc)(hWnd, message, wParam, lParam);

		// 截留模式
		return 0;
	}
	else
	{
		// 转发模式 MODE_FORWARD
		return ((WNDPROC)_wndProc)(hWnd, message, wParam, lParam);
	}
}

/*
* MyConsole
*/
MyConsole::MyConsole(int maxChars, bool autoWrap, HDC hdc)
	:MyEdit(maxChars, autoWrap, hdc)
{
	_promptStr = NULL;
	_mode = CONSOLEMODE_OUTPUT;
	_hEvWnd = NULL;
	_evMessageId = WM_CONSOLE_EVENT;
	_evMask = CONSOLE_EVENT_NONE;

	_inputStartPos = 0;
	_maxHistory = 20;
	_curHistoryIndex = 0;
}

MyConsole::~MyConsole()
{
	if(_promptStr) delete []_promptStr;
}

int MyConsole::attach(HWND hwnd, int m)
{
	return MyEdit::attach(hwnd, m);
}

HWND MyConsole::detach()
{
	return MyEdit::detach();
}

int MyConsole::setMargin(int margin, COLORREF clr)
{
	return MyEdit::setMargin(margin, clr);
}

int MyConsole::enableHistory(int maxRecords)
{
	int ov = _maxHistory;
	_maxHistory = maxRecords;
	return ov;
}

int MyConsole::clear()
{
	SendMessage(_hWnd, WM_CONSOLE_CLEAR, 0, 0);
	return 0;
}

int MyConsole::doClear()
{
	// 删除所有文本
	vremove(0, -1);

	if(CONSOLEMODE_INPUT == _mode)
	{
		_inputStartPos = 0;

		// 写命令行提示符
		appendPrompt();

		// 记录输入的起始位置
		_inputStartPos = size();
		vsetCaretPos(_inputStartPos);
	}

	refresh();
	return 0;
}

int MyConsole::setEventHandler(HWND hWnd, UINT msgid)
{
	_hEvWnd = hWnd;
	_evMessageId = msgid;
	return 0;
}

int MyConsole::setEventMask(unsigned int mask)
{
	_evMask = mask;
	return 0;
}


// 创建并显示光标
int MyConsole::vshowCaret()
{
	if(CONSOLEMODE_OUTPUT == _mode)
	{
	}
	else
	{
		// 创建控制台光标,不同于 Edit 光标
		if(!CreateCaret(_hWnd, NULL, _vImage->getCharAveWidth(), CARET_HEIGHT))
		{
			assert(0);
		}
		else
		{
			ShowCaret(_hWnd);
		}
	}
	return 0;
}

int MyConsole::vhideCaret()
{
	if(CONSOLEMODE_OUTPUT == _mode)
	{
	}
	else
	{
		DestroyCaret();
	}
	return 0;
}

// 设置光标的位置
int MyConsole::vsetCaretPos(int pos)
{
	if(CONSOLEMODE_OUTPUT == _mode) return 0;

	if(pos < _inputStartPos)
	{
		pos = _inputStartPos;
	}
	return MyEdit::vsetCaretPos(pos);
}

//// 更新光标的位置
int MyConsole::vrefreshCaret()
{
	if(CONSOLEMODE_INPUT == _mode)
	{
		assert(_curCaretPos >= 0 && _curCaretPos <= size());
		POINT pt = _vImage->getPoint(_curCaretPos);
		POINT sz = {0, _vImage->getCharHeight()};
		MyCell* cell = _vImage->getCell(_curCaretPos);
		if(cell)
		{
			sz.x = cell->width();
			sz.y = cell->height();
		}

		pt.x -= _windowPos.x;
		pt.y -= _windowPos.y;
		pt.x += _margin;
		pt.y += _margin;
		SetCaretPos(pt.x, pt.y + sz.y - CARET_HEIGHT);
	}
	return 0;
}

int MyConsole::setPrompt(const wchar_t* str)
{
	if(_promptStr)
	{
		delete []_promptStr;
		_promptStr = NULL;
	}

	if(str)
	{
		size_t len = wcslen(str);
		_promptStr = new wchar_t[len + 1];
		assert(_promptStr);
		wcscpy_s(_promptStr, len, str);
	}
	return 0;
}

// 总是在最后添加命令行提示符
void MyConsole::appendPrompt()
{
	if(NULL == _promptStr)
	{
		wchar_t prompt[64];

		time_t tt;
		time(&tt);
		struct tm tmm;
		localtime_s(&tmm, &tt);

		write(prompt, 
			swprintf(prompt, 63, L"[%04d-%02d-%02d %02d:%02d:%02d]$", tmm.tm_year + 1900, tmm.tm_mon + 1, tmm.tm_mday, tmm.tm_hour, tmm.tm_min, tmm.tm_sec), 
			_dftFgColor, _dftBkColor);
	}
	else
	{
		write(_promptStr, wcslen(_promptStr), _dftFgColor, _dftBkColor);
	}
}

int MyConsole::switchMode(int m, bool quiet)
{
	return (int)SendMessage(_hWnd, WM_CONSOLE_SWITCHMODE, m, quiet ? 1 : 0);
}

int MyConsole::doSwitchMode(int m, bool quiet)
{
	if(m == _mode) return 1;
	if(CONSOLEMODE_OUTPUT == m)
	{
		// 隐藏光标
		vhideCaret();
		
		// 切换到输出模式
		_mode = CONSOLEMODE_OUTPUT;

		// 插入一个换行符
		if(!quiet)
		{
			write(L"\r\n", 2, 0, 0);
		}
	}
	else
	{	// 追加提示符
		if(!quiet)
		{
			write(L"\r\n", 2, 0, 0);
			appendPrompt();
		}

		// 切换到输入模式
		_mode = CONSOLEMODE_INPUT;

		// 记录输入的起始位置
		_inputStartPos = size();

		// 创建光标
		vshowCaret();
		vsetCaretPos(_inputStartPos);
	}
	return 0;
}

int MyConsole::vreplaceSel(const wchar_t* ch, int len, COLORREF fg /*= CONSOLECOLOR_DEFAULT*/, COLORREF bk /*= CONSOLECOLOR_DEFAULT*/)
{
	// 删除选区内可写文本后清空选区
	int pos = 0;
	int selPos, selLen;
	getWritableSel(&selPos, &selLen);
	if(selLen > 0)
	{
		vremove(selPos, selLen);
		pos = selPos;
	}
	else
	{
		pos = vgetCaretPos();
	}
	vsetSel(0, 0);

	// 在原选区或者光标位置插入新文本
	if(len > 0)
	{
		len = vinsert(pos, ch, len, fg, bk);
	}

	// 先更新滚动条信息
	updateScrollInfo();

	// 确保光标可见
	if(!vmakeVisible(vgetCaretPos()))
	{
	}

	// 刷新窗口
	redraw();

	return len;
}

int MyConsole::read(wchar_t* buf, int len) const
{
	MYEDIT_CELLSBUF info;
	info.cell = NULL;
	info.ch = buf;
	info.chLen = len;
	info.pos = 0;
	SendMessage(_hWnd, WM_CONSOLE_READ, 0, (LPARAM)&info);
	return info.len;
}
	
int MyConsole::doRead(wchar_t* buf, int len) const
{
	if(CONSOLEMODE_INPUT == _mode)
	{
		int inputLen = vgetText(_inputStartPos, _vImage->size() - _inputStartPos, NULL, 0);
		if(NULL == buf) return inputLen;

		return vgetText(_inputStartPos, inputLen, buf, len);
	}
	else
	{
		assert(0);
		return 0;
	}
}

std::wstring MyConsole::getInput() const
{
	std::wstring inputStr;
	SendMessage(_hWnd, WM_CONSOLE_GETINPUT, 0, (LPARAM)&inputStr);
	return inputStr;
}

std::wstring MyConsole::doGetInput() const
{
	std::wstring inputStr(L"");
	int inputLen = doRead(NULL, 0);
	if(inputLen > 0)
	{
		wchar_t* buf = new wchar_t[inputLen + 1];
		doRead(buf, inputLen);
		buf[inputLen] = 0;
		inputStr = buf;
		delete []buf;
	}

	return inputStr;
}

int MyConsole::write(const wchar_t* ch, int len, const COLORREF fg, const COLORREF bk)
{
	MYEDIT_CELLS info;
	info.cell = NULL;
	info.ch = ch;
	info.len = len;
	info.pos = vgetCaretPos();
	info.fg = fg;
	info.bk = bk;
	SendMessage(_hWnd, WM_CONSOLE_WRITE, 0, (LPARAM)&info);
	return info.len;
}

int MyConsole::doWrite(const wchar_t* ch, int len, const COLORREF fg, const COLORREF bk)
{
	if(-1 == len && ch) len = wcslen(ch);

	if(CONSOLEMODE_OUTPUT == _mode)
	{
		// 直接追加到末尾并确保可见
		vsetSel(0, 0);
		MyEdit::vinsert(size(), ch, len, fg, bk);
		updateScrollInfo();
		vmakeVisible(size());
		redraw();
	}
	else
	{
		vreplaceSel(ch, len, fg, bk);
	}
	return len;
}

int MyConsole::getInputStartPos() const
{
	return _inputStartPos;
}

int MyConsole::clearInput()
{
	SendMessage(_hWnd, WM_CONSOLE_CLEAR, 1, 0);
	return 0;
}

int MyConsole::doClearInput()
{
	if(CONSOLEMODE_INPUT == _mode)
	{
		assert(_inputStartPos >= 0 && _inputStartPos <= size());

		vsetSel(_inputStartPos, size() - _inputStartPos);
		vreplaceSel(NULL, 0);
	}
	return 0;
}

int MyConsole::mode() const
{
	return _mode;
}

// 获取全部选区或者可写的选区
int MyConsole::getWritableSel(int* pos, int* len) const
{
	int selPos, selLen;
	vgetSel(&selPos, &selLen);
	if(selLen > 0)
	{
		int posFrom = selPos;
		int posTo = selPos + selLen;
		if(posFrom < _inputStartPos) posFrom = _inputStartPos;
		if(posTo < _inputStartPos) posTo = _inputStartPos;
			
		*pos = posFrom;
		*len = posTo - posFrom;
	}
	else
	{
		*pos = _inputStartPos;
		*len = 0;
	}

	return 0;
}

/*
* 只需要处理和 MyEdit 逻辑相异的部分
*/
LRESULT MyConsole::wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	assert(hWnd == _hWnd);
	switch (message)
	{
		case WM_SETCURSOR:
		{
			// 强制调用默认处理过程设置普通的箭头鼠标指针
			return ((WNDPROC)_wndProc)(hWnd, message, wParam, lParam);
		}
		break;
		case WM_KEYDOWN:
		{
			// 只拦截滚动相关的按键,其它按键事件还是有原来的窗口处理函数处理
			switch (wParam)
			{
				case VK_UP:
				{
					if(CONSOLEMODE_OUTPUT == _mode)
					{
						SendMessage(hWnd, WM_VSCROLL, SB_LINEUP, 0);
					}
					else
					{
						// 记录当前正在输入的内容
						if(_curHistoryIndex >= (int)_history.size())
						{
							_curInput = getInput();
						}
						
						// 显示上一次的输入记录
						if(_curHistoryIndex > 0)
						{
							--_curHistoryIndex;

							vsetSel(_inputStartPos, size() - _inputStartPos);
							vreplaceSel(_history[_curHistoryIndex].c_str(), _history[_curHistoryIndex].size());
						}
					}
				}
				break;
				case VK_DOWN:
				{
					if(CONSOLEMODE_OUTPUT == _mode)
					{
						SendMessage(hWnd, WM_VSCROLL, SB_LINEDOWN, 0);
					}
					else
					{
						if(_curHistoryIndex < (int)_history.size() - 1)
						{
							++_curHistoryIndex;
							vsetSel(_inputStartPos, size() - _inputStartPos);
							vreplaceSel(_history[_curHistoryIndex].c_str(), _history[_curHistoryIndex].size());
						}
						else if(_curHistoryIndex == (int)_history.size() - 1)
						{
							++_curHistoryIndex;
							vsetSel(_inputStartPos, size() - _inputStartPos);
							vreplaceSel(_curInput.c_str(), _curInput.size());
						}
						else
						{
						}
					}
				}
				break;
				default:
					return MyEdit::wndProc(hWnd, message, wParam, lParam);
			}
		}
		break;
		case WM_IME_CHAR:
		case WM_CHAR:
		{
			if(CONSOLEMODE_OUTPUT == _mode) break;
			
			// 生成事件
			int ev = CONSOLE_EVENT_CHAR;
			if(VK_RETURN == wParam)
			{
				ev |= CONSOLE_EVENT_RETRUN;

				if(GetKeyState(VK_CONTROL) < 0)
				{
					ev |= CONSOLE_EVENT_CTRL_RETRUN;
				}
				
				if(GetKeyState(VK_MENU) < 0)
				{
					ev |= CONSOLE_EVENT_ALT_RETRUN;
				}
			}

			// 必要的话把事件发送到目标窗口
			ev &= _evMask;
			if(ev)
			{
				std::wstring inputStr = doGetInput();
				HWND hDest = _hEvWnd;
				if(hDest == NULL) hDest = GetParent(_hWnd);
				if(hDest == NULL) hDest = _hWnd;
				if(0 != SendMessage(hDest, _evMessageId, ev, wParam))
				{
					// 按回车时记录历史记录
					if(VK_RETURN == wParam)
					{
						if(inputStr.empty() || _history.size() > 0 && _history.back() == inputStr)
						{
							// 和最后一条记录相同就不再记录
							_curHistoryIndex = _history.size();
						}
						else
						{
							if(_maxHistory > 0)
							{
								if((int)_history.size() >= _maxHistory)
								{
									_history.erase(_history.begin());
								}
								_history.push_back(inputStr);
								_curHistoryIndex = _history.size();
							}
						}
					}

					// 事件已经被目标窗口处理,跳出,结束
					break;
				}
			}
			
			// 事件被目标窗口忽略,继续处理
			return MyEdit::wndProc(hWnd, message, wParam, lParam);
		}
		break;
		case WM_CONSOLE_WRITE:
		{
			MYEDIT_CELLS* info = (MYEDIT_CELLS*)lParam;
			info->len = doWrite(info->ch, info->len, info->fg, info->bk);
		}
		break;
		case WM_CONSOLE_READ:
		{
			MYEDIT_CELLSBUF* info = (MYEDIT_CELLSBUF*)lParam;
			info->len = doRead(info->ch, info->chLen);
		}
		break;
		case WM_CONSOLE_GETINPUT:
		{
			std::wstring* inputStr = (std::wstring*)lParam;
			*inputStr = doGetInput();
		}
		break;
		case WM_CONSOLE_SWITCHMODE:
		{
			return doSwitchMode((int)wParam, lParam != 0);
		}
		break;
		case WM_CONSOLE_CLEAR:
		{
			if(wParam != 0)
			{
				doClearInput();
			}
			else
			{
				doClear();
			}
		}
		break;
		default:
		{
			// 其它消息调用原来的处理函数
			return MyEdit::wndProc(hWnd, message, wParam, lParam);
		}
	}

	if(MESSAGE_MODE_INTERCEPT == _messageHandleMode)
	{
		// 截留模式
		return 0;
	}
	else
	{
		// 转发模式 MODE_FORWARD
		return ((WNDPROC)_wndProc)(hWnd, message, wParam, lParam);
	}
}
