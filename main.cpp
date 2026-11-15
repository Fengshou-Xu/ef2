#define _WIN32_WINNT 0x0600
#define HEADSOCKET_IMPLEMENTATION
#include "src\headsocket.h"
#include "src\code.h"
#include "src\privilege.h"
#include "src\ui.h"
#include "src\websocket.h"

#pragma comment(linker, "/SUBSYSTEM:windows")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "OleAut32.lib")
#pragma comment(lib, "Comctl32.lib")

static bool StartsWithNoCase(const std::wstring &s, const wchar_t *prefix)
{
  return _wcsnicmp(s.c_str(), prefix, wcslen(prefix)) == 0;
}

static bool EndsWithNoCase(const std::wstring &s, const wchar_t *suffix)
{
  const size_t len = wcslen(suffix);
  return s.size() >= len && _wcsicmp(s.c_str() + s.size() - len, suffix) == 0;
}

/* 安装界面是固定像素布局，保持系统缩放；这里只让提示对话框在高分屏上不发虚（Win10 1607+） */
static void UseCrispDialogs()
{
  typedef HANDLE(WINAPI * SetContext)(HANDLE);
  SetContext set = (SetContext)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetThreadDpiAwarenessContext");
  if (set)
    set((HANDLE)-4); // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
  // 命令行调用：用宽字符参数，路径里有系统代码页之外的字符（emoji、韩文等）也能打开
  int argc = 0;
  LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  for (int i = 1; argv && i < argc; i++)
  {
    const std::wstring arg = argv[i];
    if (StartsWithNoCase(arg, L"ef2://"))
    {
      if (StartsWithNoCase(arg, L"ef2://websocket"))
      {
        Websocket websocker(arg.size() > 16 ? _wtol(arg.c_str() + 16) : 0);
        return 0;
      }
      UseCrispDialogs();
      return Code::ef2Protocol(ef2::Narrow(arg));
    }
    if (EndsWithNoCase(arg, L".ef2") || ef2::FileExistsNonEmpty(arg))
    {
      UseCrispDialogs();
      return Code::ef2File(arg);
    }
  }

  // UAC权限提升
  if (!Privilege::QueryPrivilege())
    return Privilege::UpgradePrivilege();

  UI ui(hInstance, nCmdShow);
  return 0;
}
