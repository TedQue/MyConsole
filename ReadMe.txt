一种文本编辑器和控制台实现方案
by Que's C++ Studio 阙荣文 20210602

0. 需求
在所有 Windows 标准控件中, Edit 大概是最复杂者之一.试想一下,实现 Edit 至少需要考虑以下问题:
	选择字体绘制字符
	响应键盘输入
	响应鼠标动作,准确选中指定字符
	访问系统剪贴板,支持热键 Ctrl-C, Ctrl-V 等
	计算长度宽度以正确设置滚动范围
	...
这些还仅仅是一个标准 Edit 控件的最基本功能,其中的大量细节已经有点让人望而生畏了,然而 Windows 标准控件库中还有一个名为 RichEdit 的控件,
用于插入"富格式"文本,比如带颜色的文字,图片乃至数据库对象等等.Edit控件用于 Windows GUI app 简单输入是完全够用的,RichEdit 则有些不上不下,
一般需求用不上(使用 Edit 控件足矣),复杂需求(比如,即时通讯软件的输入框,需要插入表情,图片等)又不够用(其基于 OLE 对象的接口过时且难以使用).
此外,开发 Windows GUI app 常需要在某个窗口中滚动输出大量文本,这是一个很常见的需求,比如日志输出.某些时候我甚至想要是能把系统控制台窗口直接嵌入,也的确做过类似尝试,
利用 Windows 控制台 AllocConsole(), GetConsoleWindow() 等接口并非不能做到,然终有拼凑之嫌,不够优雅.

以上就是我的3个需求:
	简单文本编辑
	富格式文本编辑
	控制台文本输入输出
我把新控件老套的命名为 MyEdit 和 MyConsole.

1. 设计思路
<<设计模式>>第二章中以一个名为"Lexi"文本编辑器作为示例演示多种模式之用,其设计思路与 MyEdit 有很多相似之处.事实上我是在 MyEdit 完成设计后的编码过程中读的<<设计模式>>
算是不谋而合,后续也受其影响,只是抽象程度不如 Lexi 简洁.

想象我们有一张很大很大的图像,先把整张图像划分为若干行,再把每行划分为若干个单元格,就像小学生使用的作文纸那样.每个单元格内画什么,如何画由单元格自身决定,可以是一个字符也可以
是一个图像.这张图像可能很大,拥有很多很多行列,远远超过屏幕尺寸,所以我们用一个尺寸比屏幕小的窗口盖在图像上,每次只显示被窗口框定的部分,通过移动窗口逐步展示整个图像,
这很好理解,因为它就是 Windows 的本意.我们把这个想象中的图像称为 MyVirtualImage, 单元格称为 MyCell.

我们在内存中维护一个 MyVirtualImage 对象,用一个二维数组记录组成行列的所有 MyCell 对象.绘制图形时, MyVirtualImage 先调用 MyCell::width()/height() 得到单元格对象 MyCell 的大小,
再根据其在行列中的位置计算出属于该单元格的绘制区域并调用其自绘接口:
void MyCell::draw(HDC hDc, const RECT* rc, bool selected).MyCell 对象可以在该区域内绘制任意图形从而使 MyEdit 具备插入富格式文本,图片,以及其他各式各样的自定义单元格的能力.
当然,并不需要把全部 MyCell 单元格都画出来,只要绘制窗口内那部分"可见" MyCell 即可,就像用记事本打开一个很大的文本文件时,每次只需显示窗口能容纳的那几页字符.

把 MyCell 对象插入 MyVirtualImage 对象相对简单,但调用者无法预期何时回收该 MyCell 对象,因为这取决于用户的鼠标键盘动作,也许一直保持,也许在用户按下 DEL, BACKSPACE, Ctrl-X
等键后删除,甚至是在达到 MyVirtualImage 设定的最大 MyCell 容量时被自动删除.为了解决回收难题, MyVirtualImage 在需要删除 MyCell 对象时调用 MyCell::remove() 接口,并保证
之后不再访问该 MyCell 对象指针. MyCell 派生类可以在 remove() 中实现自己的删除逻辑.

MyVirtualImage 插入 MyCell 接口原型为 int MyVirtualImage::insert(int pos, MyCell** cells, int len),大多数时候 MyEdit/MyConsole 都在处理 MyCell 的字符
派生类 MyCellCharacter, 如果每插入一个字符都 new 一个新的 MyCellCharacter 对象效率较低, MyCellCharacterFactory 实现了 MyCellCharacter 对象池,以"块"方式管理多个 MyCellCharacter 对象,
并跟踪该块的引用计数,重复使用从而提高效率.

上文中为了便于理解,说是用一个二维数组管理所有 MyCell 对象,实际上 MyVirtualImage 使用一维数组保存所有 MyCell 对象指针和一个额外的行索引(也是一个一维数组)记录每一行的起始 MyCell 对象在前述一维数组
中的序号.相较而言,二维数组简单易懂,行内添加删除也很高效,但是在处理大量不同长度的行时比较头疼,而行索引方案则可以很高效的处理这种情况.最终我采用了行索引方案,花费较多精力实现高效的行索引更新函数:
int MyVirtualImage::updateRowIndex(int pos, int len),细节可参考源码中该函数的注释部分.
行索引还需要跟踪每一行的宽度和高度, MyVirtualImage 统计所有行高度之和以及最大行宽度得到虚拟图像的尺寸, MyEdit 需要这个信息设置滚动条参数.

2. MyEdit
现在,我们已经有了一张想象中的虚拟图像,我们知道它的尺寸,知道每一个单元格的行列序号,每个单元格都可以正确绘制它想绘制的任意图形,还有添加修改删除单元格对象的接口.
还需要什么?还需要把虚拟图像中的某个部分(窗口)显示在某个 Windows HWND 窗口上,响应用户鼠标键盘动作操作单元格对象(大多数时候是字符单元格对象),设置正确的滚动条参数使窗口可以在虚拟图像上移动...等等
这些琐碎的用户接口相关事务正是 MyEdit 需要完成的工作.

Windows 窗口行为的关键在于其关联的消息处理函数,调用 int MyEdit::attach(HWND hwnd, int m) 把目标窗口过程函数替换为 MyEdit::wndProc() 即可把该窗口子类化为 MyEdit,之后就是在该窗口过程
函数中响应消息,其中并没有什么难点却需要十足的耐心处理各种细节.值得一提的是关于输入法预输入字符的显示与编辑,主要是响应 WM_IME_XXX 相关消息,较为少见,详情请直接阅读源码.因为这个缘故,需要在
工程中加入 Imm32.lib 库支持.

标准的 Windows 控件除了提供函数操作接口外都支持通过 Windows 消息操控,比如调用 GetWindowText() 和发送 WM_GETTEXT 消息都可以获取 Edit 控件的内容. MyEdit 也遵循这个惯例,在 MyConsole.h
中定义了一组支持的 Windows 自定义消息.

3. MyConsole
MyConsole 与 MyEdit 本质上是一样的,只是 MyConsole 需要在输入或输出两种模式间切换,并且检测到用户输入结束(按下回车键)时通知接口调用者.
所以,先用私有继承把 MyEdit 的实现包含进来,之后再额外实现模式切换和事件通知机制(简单发送 Windows 消息至父窗口)即可.

4. 添加到您的项目
MyEdit/MyConsole 直接用 Windows SDK 实现,并不需要 MFC 库支持,使用时包含以下 4 个文件: MyCell.h MyCell.cpp MyConsole.h MyConsole.cpp
如果需求更复杂的单元格功能,比如插入图片,请参考 MyCellCharacter 实现 MyCell 接口.

5. 可能的应用场景和还需要完善的地方
即时通讯软件聊天输入框和显示框;实现自己的文本编辑器;带颜色的日志输出窗口.
没有实现 undo/redo 树;没有实现拖拽功能;没有经过压力测试,性能可能比标准的 Windows 记事本低很多;只实现了最简单的字符单元格接口,要支持完整的富文本格式还缺大量工作.
总而言之,相当简陋,远远不能称之为"产品",并且本项目构建于 2015 年,彼时浮躁,诸多细节不甚考究,如今再读颇感不堪,想要重写一时又无从着手,权当引玉之砖,不足之处望各位同学海涵.

[后记]
因写此文,我用 Visual Studio 2019 社区版重新编译了项目,为了避免不必要的麻烦,也请您使用这个版本.
我个人由于工作需要,目前已转至 Linux 平台.一向穷冗,现整理两个 Windows 平台项目与各位同学交流学习.感叹今日 Windows 桌面应用程序开发之日渐式微,做这两个项目也已经是好几年前事了.
后续还有一篇关于 IOCP 与 OpenSSL 整合的文章,之后将告别 Windows 平台.

-------------------------------------------------------------------------------------------------------------------------
附1: 本项目运行效果
screenshot1.png

-------------------------------------------------------------------------------------------------------------------------
附2: MyConsole 用作日志输出窗口时的运行效果(不同日志级别输出为不同颜色)
screenshot2.png

-------------------------------------------------------------------------------------------------------------------------
附3: 开发日志,记录过程中的所思所想

对于控制台窗口,关键是 屏幕缓冲. 控制台根据屏幕缓冲来绘制屏幕.
屏幕缓冲基本上可以认为是一个二维数组,每个单元是一个数据结构,里面包含字符,前景色,背景色,字体.

如果自己实现全部过程可行不可行?

思路:
(1) 创建Console 之后, 调用 CreateFile 得到系统创建的 ScreenBuffer 这是当前活跃的 ScreenBuffer. 或者这个ScreenBuffer 不管它.
根据当前窗口的大小,另外创建两个 ScreenBuffer A, B. 把 A 设置为当前的活跃 Buffer.
在 WM_SIZE 之后,根据窗体的宽度,修改 B 的Buffer 大小,清空 B, 然后把 A 的内容复制到 B, 再把 B 设置为活跃 Buffer .
下一次 WM_SIZE , A 和 B 交换角色,重复做一遍.

(2) 响应 WM_SIZING 消息,控制窗口大小的改变始终以字符为单位一档一档变化而不是一个像素一个像素连续变化,并且设置一个最小值.

2015.3.29
添加输入功能,思路
1. size_t getText(int posFrom, size_t len, wchar_t *dest) 获取从位置 posFrom 开始的字符串.
2. int getCurrentPosition() 获取当前光标的位置.
3. int hitTest(POINT pos) 根据屏幕位置(鼠标)返回 _cells 对应的序号
4. int select(int from, int to) 选中.
5. size_t insert(int posAfter ...) 代替 write() 添加文本.

2015.3.30
ScreenBuffer 不管光标的事,光标由窗口管理. 所以 getCurrentPosition() 是没用的, 对窗口来说 getLength() 和 hitTest() 两个函数配合就可以控制光标了. .
选中区域的绘制:固定颜色的背景色/固定颜色背景色+前景色取反/背景色取反+前景色取反

22:24
POINT  getCurrentPosition() 返回下一个字符的输出位置(单位 x,y 像素) 也就是光标的位置,因为只有 ScreenBuffer 才能计算这个值.
换行符占用一个空格的位置,这样选中时才能画出.(只有在选中时,如果还有空位则画出来,否则不管)

2015.5.28
抽象 IOwnerdrawCell 接口作为 MyBuffer 里的单元格 - 才能支持各种自定义图像,动画,窗口等.

MyBuffer 是否需要 SaveDC 和 RestoreDC() ? 还是每个 IOwnerdrawCell 负责恢复原状.

POINT size(); -> 获取长和宽
void draw(HDC hDc, POINT pt); -> 重画
void show(RECT rc); -> 可见时由框架调用
void hide();	-> 不可见时由框架调用
void move(); -> 需要移动位置的时候由框架调用 - 存疑

行索引要保留行的高度和宽度了.
实现是一个类,用来显示一个字符的 Cell

如果单元格需要更新时,如何反向通知 MyBuffer?

连续输出卡顿的问题是因为程序一直在循环处理输出的代码,虽然可以不停的通过调用 UpdateWindow() 直接调用消息处理函数重画窗口,但是其它消息没有机会得到执行.
所以界面看起来就没有响应.如果每次输出若干条之后,插入消息循环代码,把消息队列中的消息处理掉之后再继续,这样是不是能好点?
这是一个思路.

删除选区之前的字符会导致选区改变的BUG
最小化恢复后显示第一行而不是最小化前的位置的 BUG - WM_SIZE 中过滤最小的情况.

2015.5.31
cell 接口: show(), hide(), select(), unselect(), draw(), setPosition(), wrap()
在 cell 中做一个反向索引, 通过 setPosition() 设置 pos 或者 row, col. 这样很多问题就迎刃而解了.
先定义接口, 用接口把 MyBuffer 改写, 在用 CharacterCell 实现 cell 接口.

10:59
事件 show,hide,select,unselect,draw,setpostion,remove,gettext, wrap,
mybuffer 不删除 cell,而是调用 remove 后返回,是否删除由cell实现决定.

由于 MyCell 和 MyConsole 之间隔着 MyBuffer, 无法实现 MyCell -> MyConsole 之间的通信,这不合原则.
具体需要有各自 MyCell 实现.

2021.6.8
v1.0.1
所有文件编码改为 UTF-8 BOM