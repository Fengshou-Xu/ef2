#ifndef REGISTER_H
#define REGISTER_H
#include "stdfx.h"
class Register
{
public:
    static BOOL CALLBACK RegQuery()
    {
        HKEY hKey;
        LSTATUS nRes;

        nRes = RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Software\\DownloadManager"), 0, KEY_READ, &hKey);
        if (nRes != ERROR_SUCCESS)
        {
            MessageBox(NULL, TEXT("您似乎未安装IDM！\r\n您必须先安装Internet Download Manager (IDM)才能使用此工具！"), NULL, MB_ICONWARNING);
            PostQuitMessage(0);
        }
        nRes = RegOpenKeyEx(HKEY_CLASSES_ROOT, TEXT("ef2"), 0, KEY_READ, &hKey);
        if (nRes != ERROR_SUCCESS)
        {
            return false;
        }

        return true;
    };
    static BOOL CALLBACK RegCreate()
    {
        BOOL state = RegQuery();
        HKEY hKey, hKey1, hKey2, hKey3;
        LPTSTR Error = 0;
        /* 命令行形如 "X:\path\ef2.exe" "%1"，路径加引号以免目录带空格时被截断 */
        TCHAR szExePath[MAX_PATH];
        TCHAR szCurPath[MAX_PATH + 16];
        ZeroMemory(szExePath, sizeof(szExePath));
        GetModuleFileName(NULL, szExePath, MAX_PATH - 1);
        _stprintf_s(szCurPath, MAX_PATH + 16, TEXT("\"%s\" \"%%1\""), szExePath);

        if (state)
        {
            /* 注销关联 */
            if (RegDeleteTree(HKEY_CLASSES_ROOT, TEXT(".ef2")) != ERROR_SUCCESS)
            {
                return false;
            }
            if (RegDeleteTree(HKEY_CLASSES_ROOT, TEXT("IDM_ef2")) != ERROR_SUCCESS)
            {
                return false;
            }
            if (RegDeleteTree(HKEY_CLASSES_ROOT, TEXT("ef2")) != ERROR_SUCCESS)
            {
                return false;
            }
            SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
            return true;
        }
        else
        {
            /* 关联.ef2 */
            if (RegCreateKey(HKEY_CLASSES_ROOT, TEXT(".ef2"), &hKey) == ERROR_SUCCESS)
            {
                RegSetValue(hKey, NULL, REG_SZ, TEXT("IDM_ef2"), 0);
                RegCloseKey(hKey);
            }
            else
            {
                return false;
            }
            /* 关联IDM_ef2 */
            if (RegCreateKey(HKEY_CLASSES_ROOT, TEXT("IDM_ef2"), &hKey) == ERROR_SUCCESS)
            {
                if (RegCreateKey(hKey, TEXT("shell"), &hKey1) == ERROR_SUCCESS)
                {
                    if (RegCreateKey(hKey1, TEXT("open"), &hKey2) == ERROR_SUCCESS)
                    {
                        if (RegCreateKey(hKey2, TEXT("command"), &hKey3) == ERROR_SUCCESS)
                        {
                            RegSetValue(hKey3, NULL, REG_SZ, szCurPath, 0);
                            RegCloseKey(hKey3);
                        }
                        else
                        {
                            RegCloseKey(hKey2);
                            RegCloseKey(hKey1);
                            RegCloseKey(hKey);
                            return false;
                        }
                        RegCloseKey(hKey2);
                    }
                    else
                    {
                        RegCloseKey(hKey1);
                        RegCloseKey(hKey);
                        return false;
                    }
                    RegCloseKey(hKey1);
                }
                else
                {
                    RegCloseKey(hKey);
                    return false;
                }
                RegCloseKey(hKey);
            }
            else
            {
                return false;
            }
            /* 关联ef2 */
            if (RegCreateKey(HKEY_CLASSES_ROOT, TEXT("ef2"), &hKey) == ERROR_SUCCESS)
            {
                RegSetValue(hKey, NULL, REG_SZ, TEXT("URL:IDM"), 0);
                RegSetValueEx(hKey, TEXT("URL Protocol"), 0, REG_SZ, NULL, 0);
                if (RegCreateKey(hKey, TEXT("shell"), &hKey1) == ERROR_SUCCESS)
                {
                    if (RegCreateKey(hKey1, TEXT("open"), &hKey2) == ERROR_SUCCESS)
                    {
                        if (RegCreateKey(hKey2, TEXT("command"), &hKey3) == ERROR_SUCCESS)
                        {
                            RegSetValue(hKey3, NULL, REG_SZ, szCurPath, 0);
                            RegCloseKey(hKey3);
                        }
                        else
                        {
                            RegCloseKey(hKey2);
                            RegCloseKey(hKey1);
                            RegCloseKey(hKey);
                            return false;
                        }
                        RegCloseKey(hKey2);
                    }
                    else
                    {
                        RegCloseKey(hKey1);
                        RegCloseKey(hKey);
                        return false;
                    }
                    RegCloseKey(hKey1);
                }
                else
                {
                    RegCloseKey(hKey);
                    return false;
                }
                RegCloseKey(hKey);
            }
            else
            {
                return false;
            }
            SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
            return true;
        }
    };
    static LPTSTR CALLBACK ErrorMessage()
    {
        LPTSTR Error = 0;

        DWORD err = GetLastError();
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                      NULL,
                      err,
                      0,
                      (LPTSTR)&Error,
                      0,
                      NULL);
        return Error;
    };
};

#endif // REGISTER_H