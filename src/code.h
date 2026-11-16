#ifndef CODE_H
#define CODE_H
#include "stdfx.h"
#include "base64.h"

#include "IDM/IDManTypeInfo.h"
#include "IDM/IDManTypeInfo_i.c"

namespace ef2
{
    /* ------------------------------------------------------------------ */
    /* 文本工具：内部一律使用 UTF-16，避免标题里的 emoji、韩文等丢字        */
    /* ------------------------------------------------------------------ */

    inline std::wstring Widen(const std::string &s, UINT codePage, DWORD flags = 0)
    {
        if (s.empty())
            return std::wstring();
        int len = MultiByteToWideChar(codePage, flags, s.data(), (int)s.size(), NULL, 0);
        if (len <= 0)
            return std::wstring();
        std::wstring out(len, L'\0');
        MultiByteToWideChar(codePage, flags, s.data(), (int)s.size(), &out[0], len);
        return out;
    }

    inline std::string Narrow(const std::wstring &s, UINT codePage = CP_UTF8)
    {
        if (s.empty())
            return std::string();
        int len = WideCharToMultiByte(codePage, 0, s.data(), (int)s.size(), NULL, 0, NULL, NULL);
        if (len <= 0)
            return std::string();
        std::string out(len, '\0');
        WideCharToMultiByte(codePage, 0, s.data(), (int)s.size(), &out[0], len, NULL, NULL);
        return out;
    }

    /** 把文件或协议里的原始字节解码为文本：UTF-16 LE（BOM）→ UTF-8 → 系统 ANSI 兜底 */
    inline std::wstring DecodeText(const std::string &bytes)
    {
        if (bytes.size() >= 2 && (unsigned char)bytes[0] == 0xFF && (unsigned char)bytes[1] == 0xFE)
        {
            std::wstring out((bytes.size() - 2) / sizeof(wchar_t), L'\0');
            if (!out.empty())
                memcpy(&out[0], bytes.data() + 2, out.size() * sizeof(wchar_t));
            return out;
        }
        const size_t skip = (bytes.size() >= 3 && bytes.compare(0, 3, "\xEF\xBB\xBF") == 0) ? 3 : 0;
        const std::string body = bytes.substr(skip);
        std::wstring text = Widen(body, CP_UTF8, MB_ERR_INVALID_CHARS);
        if (text.empty())
            text = Widen(body, CP_ACP);
        return text;
    }

    inline bool IsSpace(wchar_t c)
    {
        return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' || c == 0xFEFF;
    }

    inline std::wstring Trim(const std::wstring &s)
    {
        size_t b = 0, e = s.size();
        while (b < e && IsSpace(s[b]))
            ++b;
        while (e > b && IsSpace(s[e - 1]))
            --e;
        return s.substr(b, e - b);
    }

    inline bool IEquals(const std::wstring &a, const wchar_t *b)
    {
        return _wcsicmp(a.c_str(), b) == 0;
    }

    /** 是否形如 scheme://... */
    inline bool LooksLikeUrl(const std::wstring &s)
    {
        size_t i = 0;
        while (i < s.size() && ((s[i] >= L'a' && s[i] <= L'z') || (s[i] >= L'A' && s[i] <= L'Z') ||
                                (i > 0 && ((s[i] >= L'0' && s[i] <= L'9') || s[i] == L'+' || s[i] == L'-' || s[i] == L'.'))))
            ++i;
        return i > 0 && s.compare(i, 3, L"://") == 0;
    }

    inline void MessageBoxText(const std::wstring &text, UINT icon)
    {
        MessageBoxW(NULL, text.c_str(), L"EF2", icon | MB_OK | MB_SETFOREGROUND);
    }

    /* ------------------------------------------------------------------ */
    /* 下载任务                                                             */
    /* ------------------------------------------------------------------ */

    struct Task
    {
        std::wstring url;
        std::wstring referer;
        std::wstring userAgent;
        std::wstring cookies;
        std::wstring postData;
        std::wstring userName;
        std::wstring password;
        std::wstring filePath; // 保存目录
        std::wstring fileName; // 文件名（含拓展名）
    };

    /** IDM SendLinkToIDM2 的 lFlags */
    enum : long
    {
        kSilent = 1,    // 不弹任何确认对话框，直接开始下载
        kQueueOnly = 2, // 只添加到队列，不开始下载
    };

    /**
     * 解析 ef2 文件内容。格式：
     *   <
     *   https://example.com/file
     *   referer: ...
     *   User-Agent: ...
     *   >
     * 关键词不区分大小写，按「第一个冒号」切分，所以链接或文件名里碰巧出现关键词也不会被误判。
     */
    inline std::vector<Task> ParseEf2(const std::wstring &text)
    {
        std::vector<Task> tasks;
        Task cur;
        bool inBlock = false;
        size_t pos = 0;
        while (pos <= text.size())
        {
            size_t eol = text.find(L'\n', pos);
            if (eol == std::wstring::npos)
                eol = text.size();
            const std::wstring line = Trim(text.substr(pos, eol - pos));
            pos = eol + 1;

            if (line == L"<")
            {
                if (inBlock && !cur.url.empty())
                    tasks.push_back(cur); // 上一条缺少结尾的 >
                cur = Task();
                inBlock = true;
                continue;
            }
            if (line == L">")
            {
                if (inBlock && !cur.url.empty())
                    tasks.push_back(cur);
                inBlock = false;
                continue;
            }
            if (!inBlock || line.empty())
                continue;
            if (cur.url.empty())
            {
                if (LooksLikeUrl(line))
                {
                    cur.url = line;
                    continue;
                }
                if (line.compare(0, 2, L"//") == 0)
                {
                    cur.url = L"https:" + line; // 省略协议头
                    continue;
                }
            }
            const size_t colon = line.find(L':');
            if (colon == std::wstring::npos)
                continue;
            const std::wstring key = Trim(line.substr(0, colon));
            const std::wstring value = Trim(line.substr(colon + 1));
            if (IEquals(key, L"referer"))
                cur.referer = value;
            else if (IEquals(key, L"User-Agent"))
                cur.userAgent = value;
            else if (IEquals(key, L"cookie") || IEquals(key, L"cookies"))
                cur.cookies = value;
            else if (IEquals(key, L"postdata"))
                cur.postData = value;
            else if (IEquals(key, L"username"))
                cur.userName = value;
            else if (IEquals(key, L"password"))
                cur.password = value;
            else if (IEquals(key, L"filepath"))
                cur.filePath = value;
            else if (IEquals(key, L"filename"))
                cur.fileName = value;
        }
        if (inBlock && !cur.url.empty())
            tasks.push_back(cur);
        return tasks;
    }

    /**
     * 把 ef2 协议解码后的「命令行」切成参数。
     * 双引号包裹的参数以「引号 + 空白/结尾」收尾，值里自带的引号（如 cookies）不会把参数截断。
     */
    inline std::vector<std::wstring> Tokenize(const std::wstring &s)
    {
        std::vector<std::wstring> out;
        const size_t n = s.size();
        size_t i = 0;
        while (i < n)
        {
            while (i < n && IsSpace(s[i]))
                ++i;
            if (i >= n)
                break;
            if (s[i] == L'"')
            {
                const size_t start = ++i;
                size_t end = n;
                for (size_t j = start; j < n; ++j)
                {
                    if (s[j] == L'"' && (j + 1 == n || IsSpace(s[j + 1])))
                    {
                        end = j;
                        break;
                    }
                }
                out.push_back(s.substr(start, end - start));
                i = end + 1;
            }
            else
            {
                const size_t start = i;
                while (i < n && !IsSpace(s[i]))
                    ++i;
                out.push_back(s.substr(start, i - start));
            }
        }
        return out;
    }

    /* ------------------------------------------------------------------ */
    /* 路径                                                                 */
    /* ------------------------------------------------------------------ */

    inline std::wstring FullPath(const std::wstring &path)
    {
        DWORD len = GetFullPathNameW(path.c_str(), 0, NULL, NULL);
        if (!len)
            return path;
        std::wstring out(len, L'\0');
        len = GetFullPathNameW(path.c_str(), len, &out[0], NULL);
        out.resize(len);
        return out;
    }

    inline bool IsAbsolutePath(const std::wstring &p)
    {
        return p.size() >= 2 && (p[1] == L':' || (p[0] == L'\\' && p[1] == L'\\'));
    }

    inline bool IsDirectory(const std::wstring &path)
    {
        const DWORD attr = GetFileAttributesW(path.c_str());
        return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
    }

    inline bool FileExistsNonEmpty(const std::wstring &path)
    {
        WIN32_FILE_ATTRIBUTE_DATA data;
        if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data))
            return false;
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            return false;
        return data.nFileSizeHigh != 0 || data.nFileSizeLow != 0;
    }

    /**
     * ef2 文件对应的下载目录：ef2 所在目录下、与 ef2 同名（去掉拓展名）的子目录。
     * 如果 ef2 本来就放在同名目录里（.../标题/标题.ef2），直接用所在目录，不再套一层。
     */
    inline std::wstring TargetDirFor(const std::wstring &ef2Path)
    {
        const std::wstring full = FullPath(ef2Path);
        const size_t slash = full.find_last_of(L"\\/");
        if (slash == std::wstring::npos)
            return std::wstring();
        const std::wstring dir = full.substr(0, slash);
        std::wstring stem = full.substr(slash + 1);
        const size_t dot = stem.rfind(L'.');
        if (dot == std::wstring::npos || dot == 0)
            return dir; // 没有拓展名时同名目录会和文件本身重名
        stem.erase(dot);
        while (!stem.empty() && (stem.back() == L' ' || stem.back() == L'.'))
            stem.pop_back(); // Windows 目录名不能以空格或句点结尾
        if (stem.empty())
            return dir;
        const size_t parent = dir.find_last_of(L"\\/");
        const std::wstring dirName = parent == std::wstring::npos ? dir : dir.substr(parent + 1);
        if (IEquals(dirName, stem.c_str()))
            return dir;
        return dir + L"\\" + stem;
    }

    /** 任务自带 filepath 时优先：绝对路径原样使用，相对路径挂在 base 下 */
    inline std::wstring ResolveDir(const std::wstring &base, const std::wstring &filePath)
    {
        std::wstring fp = Trim(filePath);
        for (wchar_t &c : fp)
            if (c == L'/')
                c = L'\\';
        while (fp.size() > 3 && fp.back() == L'\\')
            fp.pop_back();
        if (fp.empty())
            return base;
        if (IsAbsolutePath(fp) || base.empty())
            return fp;
        return base + L"\\" + fp;
    }

    /** 文件名里不允许带目录，也不允许 Windows 禁用字符 */
    inline std::wstring SafeFileName(const std::wstring &name)
    {
        std::wstring out = Trim(name);
        const size_t slash = out.find_last_of(L"\\/");
        if (slash != std::wstring::npos)
            out.erase(0, slash + 1);
        for (wchar_t &c : out)
            if (c < 32 || wcschr(L"<>:\"|?*", c))
                c = L'_';
        return out;
    }

    inline bool ReadFileBytes(const std::wstring &path, std::string &out)
    {
        HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE)
            return false;
        LARGE_INTEGER size;
        bool ok = GetFileSizeEx(file, &size) && size.QuadPart <= 64 * 1024 * 1024;
        if (ok)
        {
            out.resize((size_t)size.QuadPart);
            size_t total = 0;
            while (ok && total < out.size())
            {
                DWORD got = 0;
                ok = ReadFile(file, &out[total], (DWORD)(out.size() - total), &got, NULL) && got > 0;
                total += got;
            }
        }
        CloseHandle(file);
        return ok;
    }

    /* ------------------------------------------------------------------ */
    /* 设置：ef2.exe 同目录下可选的 ef2.ini                                 */
    /* ------------------------------------------------------------------ */

    struct Settings
    {
        enum
        {
            kDefaultAskAbove = 16,
            kMaxAskAbove = 9999
        };

        bool subdir = true;              // 下载到与 ef2 文件同名的子目录
        bool skipExisting = true;        // 目标文件已存在则不重复下载
        bool keepNames = true;           // IDM 询问「是否按服务器的文件类型改名」时自动答否
        int askAbove = kDefaultAskAbove; // 任务数超过该值时先询问；0 = 从不询问

        /** ef2.exe 同目录下的 ef2.ini */
        static std::wstring IniPath()
        {
            wchar_t exe[1024] = {0};
            GetModuleFileNameW(NULL, exe, 1023);
            std::wstring ini(exe);
            const size_t dot = ini.rfind(L'.');
            if (dot != std::wstring::npos)
                ini.erase(dot);
            return ini + L".ini";
        }

        static int ClampAskAbove(int value)
        {
            return value < 0 ? 0 : value > kMaxAskAbove ? kMaxAskAbove : value;
        }

        static Settings Load()
        {
            Settings s;
            const std::wstring ini = IniPath();
            s.subdir = GetPrivateProfileIntW(L"ef2", L"subdir", 1, ini.c_str()) != 0;
            s.skipExisting = GetPrivateProfileIntW(L"ef2", L"skip_existing", 1, ini.c_str()) != 0;
            s.keepNames = GetPrivateProfileIntW(L"ef2", L"keep_names", 1, ini.c_str()) != 0;
            s.askAbove = ClampAskAbove((int)GetPrivateProfileIntW(L"ef2", L"ask_above", kDefaultAskAbove, ini.c_str()));
            return s;
        }

        /** 保存「超过多少条先询问」，供程序主窗口里的设置项使用；只改这一项，ini 里其他内容原样保留 */
        static bool SaveAskAbove(int value)
        {
            const std::wstring text = std::to_wstring(ClampAskAbove(value));
            return WritePrivateProfileStringW(L"ef2", L"ask_above", text.c_str(), IniPath().c_str()) != 0;
        }
    };

    /* ------------------------------------------------------------------ */
    /* IDM COM 接口                                                         */
    /* ------------------------------------------------------------------ */

    class Bstr
    {
    public:
        explicit Bstr(const std::wstring &s) : bstr_(s.empty() ? NULL : SysAllocStringLen(s.data(), (UINT)s.size())) {}
        ~Bstr() { SysFreeString(bstr_); }
        operator BSTR() const { return bstr_; }

    private:
        Bstr(const Bstr &);
        Bstr &operator=(const Bstr &);
        BSTR bstr_;
    };

    /** 一次连接可以连续发送多条链接，IDM 未运行时会被 COM 自动拉起 */
    class IdmLink
    {
    public:
        IdmLink() : idm_(NULL)
        {
            init_ = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
            hr_ = CoCreateInstance(CLSID_CIDMLinkTransmitter, NULL, CLSCTX_LOCAL_SERVER, IID_ICIDMLinkTransmitter2, (void **)&idm_);
        }
        ~IdmLink()
        {
            if (idm_)
                idm_->Release();
            if (SUCCEEDED(init_))
                CoUninitialize();
        }
        bool ok() const { return idm_ != NULL; }
        HRESULT error() const { return hr_; }

        HRESULT Send(const Task &task, const std::wstring &dir, long flags)
        {
            if (!idm_)
                return hr_;
            Bstr url(task.url), referer(task.referer), cookies(task.cookies), postData(task.postData);
            Bstr userName(task.userName), password(task.password), localPath(dir), fileName(task.fileName);
            Bstr userAgent(task.userAgent);
            VARIANT reserved1, reserved2;
            VariantInit(&reserved1); // reserved1 = User-Agent
            VariantInit(&reserved2);
            if (userAgent)
            {
                reserved1.vt = VT_BSTR;
                reserved1.bstrVal = userAgent;
            }
            return idm_->SendLinkToIDM2(url, referer, cookies, postData, userName, password, localPath, fileName, flags, reserved1, reserved2);
        }

    private:
        IdmLink(const IdmLink &);
        IdmLink &operator=(const IdmLink &);
        ICIDMLinkTransmitter2 *idm_;
        HRESULT init_;
        HRESULT hr_;
    };

    inline std::wstring HresultText(HRESULT hr)
    {
        wchar_t buf[32];
        swprintf_s(buf, L"0x%08X", (unsigned)hr);
        return buf;
    }

    inline void ReportIdmUnavailable(HRESULT hr)
    {
        MessageBoxText(L"拉起IDM失败！未找到IDM的COM组件，请确认已安装 Internet Download Manager。\n错误码：" + HresultText(hr), MB_ICONWARNING);
    }

    /* ------------------------------------------------------------------ */
    /* IDM 的「文件类型与文件名不符」询问                                   */
    /* ------------------------------------------------------------------ */

    /*
     * 服务器声明的类型和文件名的拓展名对不上时，IDM 会弹窗问「是否改用服务器提供的类型保存」，
     * 静默标志也拦不住。B站的音频流正是如此：文件名是 xxx.m4a，服务器却说它是 video/mp4。
     * 答「是」会把音频存成 .mp4，和同名的视频文件撞名；不答下载就一直卡着。
     * 所以把任务发给 IDM 之后再守一小会儿，替用户对这类询问答「否」，保留 ef2 里写的文件名。
     */

    struct SentFile
    {
        std::wstring name; // 发给 IDM 的文件名
        std::wstring dir;  // 保存目录，可能为空
    };

    inline std::wstring WindowText(HWND hwnd)
    {
        const int len = GetWindowTextLengthW(hwnd);
        if (len <= 0)
            return std::wstring();
        std::wstring text(len + 1, L'\0');
        text.resize(GetWindowTextW(hwnd, &text[0], len + 1));
        return text;
    }

    inline bool IsIdmWindow(HWND hwnd)
    {
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!process)
            return false;
        wchar_t path[1024] = {0};
        DWORD size = 1023;
        const bool ok = QueryFullProcessImageNameW(process, 0, path, &size) != 0;
        CloseHandle(process);
        if (!ok)
            return false;
        const wchar_t *base = wcsrchr(path, L'\\');
        return _wcsicmp(base ? base + 1 : path, L"IDMan.exe") == 0;
    }

    inline BOOL CALLBACK CollectStaticText(HWND child, LPARAM param)
    {
        wchar_t cls[32] = {0};
        GetClassNameW(child, cls, 31);
        if (_wcsicmp(cls, L"Static") == 0)
            *reinterpret_cast<std::wstring *>(param) += WindowText(child) + L"\n";
        return TRUE;
    }

    /** 文字里是否提到了一个 MIME 类型（video/mp4 之类）。只认这种询问，别的是/否对话框一概不碰 */
    inline bool MentionsMimeType(const std::wstring &text)
    {
        if (text.find(L"://") != std::wstring::npos)
            return false; // 带网址的对话框不是我们要找的
        std::wstring lower(text);
        for (wchar_t &c : lower)
            if (c >= L'A' && c <= L'Z')
                c = (wchar_t)(c - L'A' + L'a');
        static const wchar_t *const kinds[] = {L"video/", L"audio/", L"application/", L"image/", L"text/"};
        for (const wchar_t *kind : kinds)
        {
            const size_t pos = lower.find(kind);
            if (pos == std::wstring::npos)
                continue;
            const size_t after = pos + wcslen(kind);
            const bool cleanStart = pos == 0 || !(lower[pos - 1] >= L'a' && lower[pos - 1] <= L'z');
            const bool hasSubtype = after < lower.size() &&
                                    ((lower[after] >= L'a' && lower[after] <= L'z') || (lower[after] >= L'0' && lower[after] <= L'9'));
            if (cleanStart && hasSubtype)
                return true;
        }
        return false;
    }

    struct PromptScan
    {
        std::vector<SentFile> *pending; // 还可能弹出询问的文件
        std::vector<HWND> *answered;    // 已经答过的对话框，避免在它关闭前重复点击
    };

    inline BOOL CALLBACK ScanIdmWindow(HWND hwnd, LPARAM param)
    {
        PromptScan *scan = reinterpret_cast<PromptScan *>(param);
        std::vector<SentFile> &pending = *scan->pending;
        if (pending.empty())
            return FALSE;
        if (!IsWindowVisible(hwnd))
            return TRUE;
        wchar_t cls[32] = {0};
        GetClassNameW(hwnd, cls, 31);
        if (wcscmp(cls, L"#32770") != 0 || !IsIdmWindow(hwnd))
            return TRUE;
        const std::wstring title = WindowText(hwnd);

        // 进度窗口的标题是「NN% 文件名」：已经在传数据，这个文件不会再有询问
        size_t digits = 0;
        while (digits < title.size() && title[digits] >= L'0' && title[digits] <= L'9')
            ++digits;
        if (digits > 0 && title.compare(digits, 2, L"% ") == 0)
        {
            const std::wstring name = title.substr(digits + 2);
            for (size_t i = pending.size(); i-- > 0;)
                if (pending[i].name == name)
                    pending.erase(pending.begin() + i);
            return TRUE;
        }

        // 询问框：标题就是 IDM 的名字，只有「是」「否」两个按钮，正文同时提到我们的文件名和一个 MIME 类型
        if (title != L"Internet Download Manager" || !GetDlgItem(hwnd, IDYES) || !GetDlgItem(hwnd, IDNO))
            return TRUE;
        for (HWND done : *scan->answered)
            if (done == hwnd)
                return TRUE;
        std::wstring text;
        EnumChildWindows(hwnd, CollectStaticText, (LPARAM)&text);
        if (!MentionsMimeType(text))
            return TRUE;
        for (size_t i = 0; i < pending.size(); ++i)
        {
            if (text.find(pending[i].name) == std::wstring::npos)
                continue;
            PostMessageW(hwnd, WM_COMMAND, MAKEWPARAM(IDNO, BN_CLICKED), (LPARAM)GetDlgItem(hwnd, IDNO));
            scan->answered->push_back(hwnd);
            pending.erase(pending.begin() + i);
            break;
        }
        return TRUE;
    }

    /**
     * 守着刚发给 IDM 的文件，直到每个文件都已答过询问、开始传数据或已下载完成；最多等 timeoutMs。
     * 超时后还没弹出的询问就只能由用户自己回答了。
     */
    inline void KeepFileNames(std::vector<SentFile> pending, DWORD timeoutMs = 60000)
    {
        std::vector<HWND> answered;
        PromptScan scan = {&pending, &answered};
        const DWORD start = GetTickCount();
        while (GetTickCount() - start < timeoutMs)
        {
            for (size_t i = pending.size(); i-- > 0;)
                if (!pending[i].dir.empty() && FileExistsNonEmpty(pending[i].dir + L"\\" + pending[i].name))
                    pending.erase(pending.begin() + i);
            if (pending.empty())
                break;
            EnumWindows(ScanIdmWindow, (LPARAM)&scan);
            if (pending.empty())
                break;
            Sleep(60);
        }
    }

    /* ------------------------------------------------------------------ */
    /* 批量任务过多时的询问                                                 */
    /* ------------------------------------------------------------------ */

    enum class BatchChoice
    {
        StartAll,
        QueueOnly,
        Cancel
    };

    inline BatchChoice AskLargeBatch(size_t count, const std::wstring &dir)
    {
        const std::wstring title = L"这个 ef2 文件包含 " + std::to_wstring(count) + L" 个下载任务";
        std::wstring content = L"全部同时开始会占用较多连接，IDM 也会为每个任务弹出进度窗口。";
        if (!dir.empty())
            content += L"\n\n保存到：" + dir;

        const int idStart = 100, idQueue = 101;
        const TASKDIALOG_BUTTON buttons[] = {
            {idStart, L"全部立即开始下载\n不再询问，IDM 同时下载所有文件"},
            {idQueue, L"只添加到 IDM 队列\n稍后自己到 IDM 里开始队列"},
        };
        TASKDIALOGCONFIG config = {0};
        config.cbSize = sizeof(config);
        config.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
        config.dwCommonButtons = TDCBF_CANCEL_BUTTON;
        config.pszWindowTitle = L"EF2";
        config.pszMainIcon = TD_INFORMATION_ICON;
        config.pszMainInstruction = title.c_str();
        config.pszContent = content.c_str();
        config.pszFooter = L"超过多少条才询问，可以直接打开 ef2.exe 修改（填 0 则从不询问）。";
        config.cButtons = ARRAYSIZE(buttons);
        config.pButtons = buttons;
        config.nDefaultButton = IDCANCEL; // 弹窗会抢焦点，默认按钮设为取消，免得被正在敲的回车/空格误触发

        int pressed = 0;
        if (FAILED(TaskDialogIndirect(&config, &pressed, NULL, NULL)))
        {
            const std::wstring text = title + L"。\n\n「是」：全部立即开始下载\n「否」：只添加到 IDM 队列\n「取消」：什么都不做";
            const int answer = MessageBoxW(NULL, text.c_str(), L"EF2", MB_YESNOCANCEL | MB_ICONQUESTION | MB_SETFOREGROUND);
            pressed = answer == IDYES ? idStart : answer == IDNO ? idQueue : IDCANCEL;
        }
        if (pressed == idStart)
            return BatchChoice::StartAll;
        if (pressed == idQueue)
            return BatchChoice::QueueOnly;
        return BatchChoice::Cancel;
    }

    /* ------------------------------------------------------------------ */
    /* 入口：双击 ef2 文件                                                  */
    /* ------------------------------------------------------------------ */

    /**
     * 打开 ef2 文件：在 ef2 旁边建同名目录，把所有任务静默发给 IDM 并立即开始下载到该目录。
     * @return 0 成功；非 0 失败或部分失败
     */
    inline int OpenFile(const std::wstring &path)
    {
        std::string bytes;
        if (!ReadFileBytes(path, bytes))
        {
            MessageBoxText(L"无法读取 ef2 文件：\n" + path, MB_ICONERROR);
            return 1;
        }
        std::vector<Task> tasks = ParseEf2(DecodeText(bytes));
        if (tasks.empty())
        {
            MessageBoxText(L"该ef2文件似乎并不包含有效下载链接！？\n请检查文件内容或者反馈到：https://github.com/MotooriKashin/ef2/issues", MB_ICONERROR);
            return 1;
        }

        const Settings settings = Settings::Load();
        const std::wstring target = settings.subdir ? TargetDirFor(path) : std::wstring();

        struct Job
        {
            const Task *task;
            std::wstring dir;
        };
        std::vector<Job> jobs;
        for (Task &task : tasks)
        {
            task.fileName = SafeFileName(task.fileName);
            const std::wstring dir = ResolveDir(target, task.filePath);
            if (settings.skipExisting && !dir.empty() && !task.fileName.empty() && FileExistsNonEmpty(dir + L"\\" + task.fileName))
                continue;
            const Job job = {&task, dir};
            jobs.push_back(job);
        }
        if (jobs.empty())
        {
            MessageBoxText(L"这个 ef2 文件里的 " + std::to_wstring(tasks.size()) + L" 个文件都已经下载过了，没有需要下载的内容。\n\n目录：" + target +
                               L"\n\n想重新下载请先删除对应文件。",
                           MB_ICONINFORMATION);
            return 0;
        }

        long flags = kSilent;
        if (settings.askAbove > 0 && jobs.size() > (size_t)settings.askAbove)
        {
            const BatchChoice choice = AskLargeBatch(jobs.size(), target);
            if (choice == BatchChoice::Cancel)
                return 0;
            if (choice == BatchChoice::QueueOnly)
                flags |= kQueueOnly;
        }

        IdmLink idm;
        if (!idm.ok())
        {
            ReportIdmUnavailable(idm.error());
            return 1;
        }

        bool createdTarget = false;
        if (!target.empty() && !IsDirectory(target))
        {
            SHCreateDirectoryExW(NULL, target.c_str(), NULL);
            if (!IsDirectory(target))
            {
                MessageBoxText(L"无法创建下载目录：\n" + target, MB_ICONERROR);
                return 1;
            }
            createdTarget = true;
        }

        size_t sent = 0;
        HRESULT lastError = S_OK;
        std::vector<SentFile> named;
        for (const Job &job : jobs)
        {
            const HRESULT hr = idm.Send(*job.task, job.dir, flags);
            if (FAILED(hr))
            {
                lastError = hr;
                continue;
            }
            ++sent;
            if (!job.task->fileName.empty())
            {
                const SentFile file = {job.task->fileName, job.dir};
                named.push_back(file);
            }
        }
        if (sent == 0 && createdTarget)
            RemoveDirectoryW(target.c_str()); // 只会删掉空目录
        if (settings.keepNames && !(flags & kQueueOnly))
            KeepFileNames(named);

        if (sent < jobs.size())
        {
            MessageBoxText(L"有 " + std::to_wstring(jobs.size() - sent) + L" / " + std::to_wstring(jobs.size()) +
                               L" 个任务没能发送给 IDM。\n错误码：" + HresultText(lastError),
                           MB_ICONWARNING);
            return 1;
        }
        if (flags & kQueueOnly)
            MessageBoxText(L"已将 " + std::to_wstring(sent) + L" 个任务添加到 IDM 下载队列，需要下载时请到 IDM 里开始队列。", MB_ICONINFORMATION);
        return 0;
    }

    /* ------------------------------------------------------------------ */
    /* 入口：ef2:// 协议                                                    */
    /* ------------------------------------------------------------------ */

    /**
     * 处理 ef2://<Base64> 链接，Base64 解码后是 UTF-8 的命令行参数（-u URL -r referer ...）。
     * @return 0 成功；非 0 失败
     */
    inline int OpenProtocol(const std::string &link)
    {
        std::string body = link;
        const size_t scheme = body.find("://");
        if (scheme != std::string::npos)
            body.erase(0, scheme + 3);
        while (!body.empty() && (body.back() == ' ' || body.back() == '"' || body.back() == '\r' || body.back() == '\n'))
            body.pop_back();
        if (body.size() % 4 == 1 && body.back() == '/')
            body.pop_back(); // 浏览器可能在末尾补一个斜杠

        std::string decoded;
        Base64::Decode(body, &decoded);
        while (!decoded.empty() && decoded.back() == '\0')
            decoded.pop_back();
        const std::vector<std::wstring> args = Tokenize(DecodeText(decoded));

        Task task;
        long flags = 0;
        for (size_t i = 0; i < args.size(); ++i)
        {
            const std::wstring &key = args[i];
            if (key.size() != 2 || key[0] != L'-')
                continue;
            std::wstring *value = NULL;
            switch (key[1])
            {
            case L'u': value = &task.url; break;
            case L'r': value = &task.referer; break;
            case L'a': value = &task.userAgent; break;
            case L'c': value = &task.cookies; break;
            case L'd': value = &task.postData; break;
            case L'U': value = &task.userName; break;
            case L'P': value = &task.password; break;
            case L'o': value = &task.filePath; break;
            case L's': value = &task.fileName; break;
            case L'f': flags |= kSilent; break;
            case L'q': flags |= kQueueOnly; break;
            }
            if (value && i + 1 < args.size())
                *value = args[++i];
        }
        if (task.url.compare(0, 2, L"//") == 0)
            task.url = L"https:" + task.url;
        if (task.url.empty())
        {
            MessageBoxText(L"非法ef2链接！请查阅相关文档：https://github.com/MotooriKashin/ef2", MB_ICONERROR);
            return 1;
        }
        task.fileName = SafeFileName(task.fileName);

        IdmLink idm;
        if (!idm.ok())
        {
            ReportIdmUnavailable(idm.error());
            return 1;
        }
        const HRESULT hr = idm.Send(task, ResolveDir(std::wstring(), task.filePath), flags);
        if (FAILED(hr))
        {
            MessageBoxText(L"拉起IDM失败！IDM返回信息异常！\n错误码：" + HresultText(hr), MB_ICONWARNING);
            return 1;
        }
        if ((flags & kQueueOnly) && !(flags & kSilent))
            MessageBoxText(L"已添加到下载队列，需要开始下载时请到IDM点击开始队列！", MB_ICONINFORMATION);
        if (flags == kSilent && !task.fileName.empty() && Settings::Load().keepNames)
        {
            const SentFile file = {task.fileName, ResolveDir(std::wstring(), task.filePath)};
            KeepFileNames(std::vector<SentFile>(1, file));
        }
        return 0;
    }
}

/** 保留原有的调用入口，供 main.cpp 与 websocket 模式使用 */
class Code
{
public:
    static int ef2Protocol(const std::string &link) { return ef2::OpenProtocol(link); }
    static int ef2File(const std::wstring &path) { return ef2::OpenFile(path); }
};

#endif // CODE_H
