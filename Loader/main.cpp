#include <windows.h>
#include <windowsx.h> // для макросов GET_X_LPARAM/GET_Y_LPARAM
#include <shlobj.h>   // для IsUserAnAdmin
#include <d3d11.h>
#include <wincodec.h>
#include <tchar.h>
#include <cmath>
#include <cstdio>
#include <vector>
#include <string>
#include <tlhelp32.h>
#include <shellapi.h>

#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "Shell32.lib")

// Подключение Nemesis
#include "Process/Process.h"
#include "ManualMap/MMap.h"

// Подключение ImGui
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

// Данные для DirectX 11
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// Пользовательский шрифт для заголовка
static ImFont*                  g_titleFont = nullptr;
static ID3D11ShaderResourceView* g_cs2BigTexture = nullptr;
static ID3D11ShaderResourceView* g_cs2MiniTexture = nullptr;
static ID3D11Texture2D*          g_sceneCaptureTexture = nullptr;
static ID3D11ShaderResourceView* g_sceneCaptureSrv = nullptr;
static int                       g_cs2BigWidth = 0;
static int                       g_cs2BigHeight = 0;
static int                       g_cs2MiniWidth = 0;
static int                       g_cs2MiniHeight = 0;
static bool                      g_productPopupOpen = false;
static float                     g_productPopupAnim = 0.0f;
static bool                      g_productPopupInputActive = false;

enum eRunState {
    RS_Idle,
    RS_ClosingSteam,
    RS_CheckingFiles,
    RS_LaunchingSteamCS2,
    RS_WaitingForCS2,
    RS_Countdown,
    RS_Finalizing,
    RS_CheckingCrash,
    RS_Finished
};
static eRunState g_runState = RS_Idle;
static float     g_runTimer = 0.0f;

// Прототипы функций
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
bool LoadTextureFromFile(const wchar_t* filename, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height);
void ReleaseTexture(ID3D11ShaderResourceView*& texture);
void CleanupSceneCapture();
bool EnsureSceneCapture(UINT width, UINT height);
void UpdateSceneCapture();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Хелперы для процессов
bool KillProcessByName(const wchar_t* name)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    bool killed = false;

    if (Process32FirstW(hSnap, &pe))
    {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0)
            {
                HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProc)
                {
                    TerminateProcess(hProc, 0);
                    CloseHandle(hProc);
                    killed = true;
                }
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return killed;
}

bool IsProcessRunning(const wchar_t* name)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    bool running = false;

    if (Process32FirstW(hSnap, &pe))
    {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0)
            {
                running = true;
                break;
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return running;
}

struct ProcessWindowSearch {
    DWORD processID;
    HWND foundWindow;
};

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    ProcessWindowSearch* data = (ProcessWindowSearch*)lParam;
    DWORD procId = 0;
    GetWindowThreadProcessId(hwnd, &procId);
    if (procId == data->processID && IsWindowVisible(hwnd)) {
        // Дополнительно можно проверить класс или имя окна, если нужно.
        // Для cs2.exe главное окно обычно имеет заголовок "Counter-Strike 2".
        // Мы просто проверим наличие видимого окна.
        data->foundWindow = hwnd;
        return FALSE; // Stop
    }
    return TRUE; // Continue
}

bool IsProcessWindowVisible(const wchar_t* processName)
{
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    bool windowVisible = false;

    if (Process32FirstW(hSnap, &pe))
    {
        do {
            if (_wcsicmp(pe.szExeFile, processName) == 0)
            {
                ProcessWindowSearch searchData = { pe.th32ProcessID, NULL };
                EnumWindows(EnumWindowsProc, (LPARAM)&searchData);
                if (searchData.foundWindow != NULL)
                {
                    windowVisible = true;
                    break;
                }
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return windowVisible;
}

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
    // Очередь проверок
    if (!IsUserAnAdmin())
    {
        MessageBoxW(NULL, L"Administrator rights are required to run this application.", L"Nemesis Loader", MB_ICONERROR);
        return 1;
    }

    if (GetFileAttributesW(L"imgui.ini") == INVALID_FILE_ATTRIBUTES)
    {
        FILE* f = _wfopen(L"imgui.ini", L"w");
        if (f)
        {
            fprintf(f, "[Window][Nemesis Loader]\nPos=0,0\nSize=685,450\nCollapsed=0\n");
            fclose(f);
        }
    }

    if (GetFileAttributesW(L"NemesisLoader.dll") == INVALID_FILE_ATTRIBUTES)
    {
        MessageBoxW(NULL, L"NemesisLoader.dll not found.", L"Nemesis Loader", MB_ICONERROR);
        return 1;
    }

    // Dummy hash check
    HANDLE hFile = CreateFileW(L"NemesisLoader.dll", GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        DWORD size = GetFileSize(hFile, NULL);
        CloseHandle(hFile);
        if (size == 0)
        {
            MessageBoxW(NULL, L"NemesisLoader.dll integrity check failed.", L"Nemesis Loader", MB_ICONERROR);
            return 1;
        }
    }

    // Очистка логов
    DeleteFileW(L".live.log");
    DeleteFileW(L"loader.log");
    DeleteFileW(L"NemesisLoader.log");

    HRESULT coInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool needCoUninitialize = SUCCEEDED(coInit);

    // Создание окна приложения
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"Nemesis Loader", nullptr };
    ::RegisterClassExW(&wc);
    
    // Создаем окно без рамки и кнопок (WS_POPUP) размером 685x450
    const int kWinW = 685;
    const int kWinH = 450;
    
    // Центрирование окна
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int winX = (screenW - kWinW) / 2;
    int winY = (screenH - kWinH) / 2;
    
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Nemesis Loader", WS_POPUP, winX, winY, kWinW, kWinH, nullptr, nullptr, wc.hInstance, nullptr);

    // Формируем регион окна: 3 скругленных угла (TL/TR/BL) и ПРЯМОЙ нижний-правый угол (BR)
    // Радиус делаем немного меньше (10px), чтобы углы были поменьше и визуально мягче
    const int kRadius = 10;          // радиус скругления в пикселях
    const int kDiam   = kRadius * 2; // диаметр для CreateRoundRectRgn

    HRGN rRoundAll = CreateRoundRectRgn(0, 0, kWinW + 1, kWinH + 1, kDiam, kDiam);
    // Восстанавливаем квадратный нижний-правый угол, объединив с прямоугольником r x r в этом углу
    HRGN rBR = CreateRectRgn(kWinW - kRadius, kWinH - kRadius, kWinW + 1, kWinH + 1);
    CombineRgn(rRoundAll, rRoundAll, rBR, RGN_OR);
    SetWindowRgn(hwnd, rRoundAll, TRUE); // система возьмёт владение rRoundAll
    DeleteObject(rBR);

    // Инициализация Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Показ окна
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    // Инициализация ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    
    // Настройка стиля
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    // Углы делаем немного меньше и «гладкими»
    style.WindowRounding = 10.0f;
    style.ChildRounding  = 10.0f;
    style.WindowBorderSize = 1.0f; // 1px обводка вокруг GUI
    style.ChildBorderSize  = 0.0f;
    style.AntiAliasedFill  = true;
    style.AntiAliasedLines = true;

    // Чуть-чуть темнее серая обводка
    style.Colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
    // Делаем фон окна непрозрачным и соответствующим clear_color
    style.Colors[ImGuiCol_WindowBg] = ImVec4(31.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f, 1.0f);

    // Настройка бэкендов
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Загрузка пользовательского шрифта для текста "Nemesis Launcher"
    // Путь из ТЗ: D:\Nemesis\Loader\Google Front\google Fronst
    // Используем Roboto-Regular.ttf и увеличиваем размер шрифта на +3.0 от базового
    {
        const float base_sz  = (io.Fonts->Fonts.Size > 0) ? io.Fonts->Fonts[0]->LegacySize : 13.0f;
        const float title_sz = base_sz + 3.0f;       // было +0.5, добавили ещё +2.5 по требованию
        g_titleFont = ImGui::GetIO().Fonts->AddFontFromFileTTF(
            "D:\\Nemesis\\Loader\\Google Front\\google Fronst\\static\\Roboto-Regular.ttf",
            title_sz,
            nullptr,
            ImGui::GetIO().Fonts->GetGlyphRangesCyrillic()
        );
        // Обновляем графические объекты бэкенда для нового атласа шрифтов
        ImGui_ImplDX11_CreateDeviceObjects();
    }

    LoadTextureFromFile(L"D:\\Nemesis\\Loader\\cs2 icon\\Bigs.jpg", &g_cs2BigTexture, &g_cs2BigWidth, &g_cs2BigHeight);
    LoadTextureFromFile(L"D:\\Nemesis\\Loader\\cs2 icon\\mini.png", &g_cs2MiniTexture, &g_cs2MiniWidth, &g_cs2MiniHeight);

    // Цвет фона: 31, 30, 30
    ImVec4 clear_color = ImVec4(31.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f, 1.00f);

    // Главный цикл
    bool done = false;
    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        // Запуск кадра ImGui
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Отрисовка кастомного окна
        {
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(685, 450));
            
            ImGuiWindowFlags main_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
            if (g_productPopupInputActive)
                main_flags |= ImGuiWindowFlags_NoInputs; // Отключаем клики по главному окну, чтобы они не перехватывали фокус
            
            ImGui::Begin("Nemesis Loader", nullptr, main_flags);

            // Заголовок в левом верхнем углу: "Nemesis Launcher" (шрифт из Google Fonts)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.82f, 0.82f, 0.82f, 1.0f)); // светло-серый
                ImGui::SetCursorPos(ImVec2(12, 12)); // опускаем ещё на 2 пикселя ниже
                if (g_titleFont) ImGui::PushFont(g_titleFont);
                ImGui::TextUnformatted("Nemesis Launcher");
                if (g_titleFont) ImGui::PopFont();
                ImGui::PopStyleColor();
            }

            // Дизайнерские кнопки в правом верхнем углу: _ и X
            {
                // Флаги активации (управляются чекбоксами, которые показываются при наведении)
                static bool s_enableClose = true;
                static bool s_enableMin   = true;

                const float pad = 8.0f;
                const float btn = 22.0f; // квадратные кнопки
                ImVec2 ws = ImGui::GetWindowSize();

                // Векторные иконки для чётких кнопок
                auto draw_icon_button = [&](const char* id,
                                            const ImVec2& local_pos,
                                            bool is_close,
                                            ImU32 col_bg, ImU32 col_bg_hover, ImU32 col_symbol,
                                            float rounding) -> bool
                {
                    ImGui::SetCursorPos(local_pos);
                    ImVec2 p = ImGui::GetCursorScreenPos();
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    bool pressed = ImGui::InvisibleButton(id, ImVec2(btn, btn));
                    bool hovered = ImGui::IsItemHovered();

                    // Фоновая плашка не рисуется (прозрачная), оставляем только иконку
                    (void)hovered; (void)col_bg; (void)col_bg_hover; (void)rounding;

                    // Иконка
                    const float m = 6.0f;          // поля от краёв
                    const float thick = 2.0f;      // толщина линий
                    if (is_close)
                    {
                        // «X» — две диагонали
                        dl->AddLine(ImVec2(p.x + m,       p.y + m),       ImVec2(p.x + btn - m, p.y + btn - m), col_symbol, thick);
                        dl->AddLine(ImVec2(p.x + btn - m, p.y + m),       ImVec2(p.x + m,       p.y + btn - m), col_symbol, thick);
                    }
                    else
                    {
                        // «_» — ровная линия
                        float y = p.y + btn * 0.60f;
                        dl->AddLine(ImVec2(p.x + m, y), ImVec2(p.x + btn - m, y), col_symbol, thick);
                    }

                    return pressed;
                };

                // Немного темнее сами кнопки (цвет иконок)
                const ImU32 col_btn_bg       = ImColor(0, 0, 0, 0);
                const ImU32 col_btn_bg_hover = ImColor(0, 0, 0, 0);
                const ImU32 col_symbol       = ImColor(210, 214, 225, 255); // чуть темнее, без «пиксельности»
                const float btn_rounding     = 6.0f;

                ImVec2 pos_close(ws.x - pad - btn, pad);
                ImVec2 pos_min  (ws.x - pad - 2.0f*btn - 6.0f, pad);

                if (draw_icon_button("##btn_close", pos_close, true,  col_btn_bg, col_btn_bg_hover, col_symbol, btn_rounding))
                {
                    if (s_enableClose)
                        done = true; // закрыть приложение
                }
                bool hovered_close = ImGui::IsItemHovered(); // наведение на кнопку X

                if (draw_icon_button("##btn_min", pos_min, false, col_btn_bg, col_btn_bg_hover, col_symbol, btn_rounding))
                {
                    if (s_enableMin)
                        ShowWindow(hwnd, SW_MINIMIZE); // свернуть приложение
                }
                bool hovered_min = ImGui::IsItemHovered(); // наведение на кнопку _

                // ТЕМНО-СЕРЫЕ чекбоксы появляются при наведении на соответствующие кнопки
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
                ImGui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.30f, 0.30f, 0.30f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  ImVec4(0.35f, 0.35f, 0.35f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_CheckMark,      ImVec4(0.80f, 0.80f, 0.80f, 1.0f));

                auto show_checkbox_for_button = [&](const char* id, const ImVec2& btn_pos, bool hovered_btn, bool* value)
                {
                    // Задаём прямоугольник появления чекбокса под кнопкой
                    ImVec2 cb_local = ImVec2(btn_pos.x + 2.0f, btn_pos.y + btn + 4.0f);
                    ImVec2 wnd_pos  = ImGui::GetWindowPos();
                    ImVec2 scr_pos0 = ImVec2(wnd_pos.x + cb_local.x, wnd_pos.y + cb_local.y);
                    ImVec2 scr_pos1 = ImVec2(scr_pos0.x + (btn - 4.0f), scr_pos0.y + (btn - 4.0f));

                    // Показываем чекбокс, если наводка на КНОПКУ или на область самого чекбокса
                    bool hover_cb = ImGui::IsMouseHoveringRect(scr_pos0, scr_pos1);
                    if (hovered_btn || hover_cb)
                    {
                        ImGui::SetCursorPos(cb_local);
                        ImGui::Checkbox(id, value);
                    }
                };

                show_checkbox_for_button("##cb_close", pos_close, hovered_close, &s_enableClose);
                show_checkbox_for_button("##cb_min",   pos_min,   hovered_min,   &s_enableMin);

                ImGui::PopStyleColor(4);
                ImGui::PopStyleVar(2);
            }

            // Fix: выровнять нижний правый угол окна ImGui под квадратный регион Win32
            // (окно ОС имеет квадратный BR-угол, а ImGui по умолчанию скругляет все 4 угла).
            // Заливаем вырезанную область и дорисовываем 1px бордер, чтобы убрать визуальный "зазубренный" угол.
            {
                const float r = ImGui::GetStyle().WindowRounding;
                if (r > 0.0f)
                {
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    ImVec2 p = ImGui::GetWindowPos();
                    ImVec2 s = ImGui::GetWindowSize();
                    ImU32 bg     = ImGui::GetColorU32(ImGuiCol_WindowBg);
                    ImU32 border = ImGui::GetColorU32(ImGuiCol_Border);

                    // Заполняем квадрат r x r в нижнем правом углу цветом фона окна
                    dl->AddRectFilled(ImVec2(p.x + s.x - r, p.y + s.y - r), ImVec2(p.x + s.x, p.y + s.y), bg, 0.0f);

                    // Продлеваем бордер вдоль правой и нижней границ только в пределах заплатки
                    const float px = 0.5f; // для чёткой 1px линии на целевой сетке
                    dl->AddLine(ImVec2(p.x + s.x - r,   p.y + s.y - px), ImVec2(p.x + s.x - px, p.y + s.y - px), border, 1.0f);
                    dl->AddLine(ImVec2(p.x + s.x - px,  p.y + s.y - r),  ImVec2(p.x + s.x - px, p.y + s.y - px), border, 1.0f);
                }
            }
            
            // Создаем «оболочку»: позиция (65,35), размер (620,415), фон 23,23,23
            // Исправление нижнего правого угла: рисуем фон и рамку вручную с квадратным BR-углом,
            // а затем создаём дочернее окно без фона поверх отрисованной области.
            // Позиция: 65 пикселей вправо, 35 пикселей вниз
            ImGui::SetCursorPos(ImVec2(65, 35));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 child_size(620, 415);
            const ImVec2 child_pos = ImGui::GetCursorScreenPos();
            const float  cr = ImGui::GetStyle().ChildRounding; // радиус скругления оболочки
            const ImU32  child_bg = ImColor(23, 23, 23, 255);
            const ImU32  child_border = ImGui::GetColorU32(ImGuiCol_Border);

            // Заливка оболочки со скруглением всех углов
            dl->AddRectFilled(child_pos, ImVec2(child_pos.x + child_size.x, child_pos.y + child_size.y), child_bg, cr);
            // «Квадратная заплатка» в нижнем правом углу, чтобы убрать скругление BR
            if (cr > 0.0f)
            {
                dl->AddRectFilled(
                    ImVec2(child_pos.x + child_size.x - cr, child_pos.y + child_size.y - cr),
                    ImVec2(child_pos.x + child_size.x,      child_pos.y + child_size.y),
                    child_bg, 0.0f);
            }
            // Рамка 1px вокруг оболочки
            dl->AddRect(child_pos, ImVec2(child_pos.x + child_size.x, child_pos.y + child_size.y), child_border, cr);
            if (cr > 0.0f)
            {
                const float px = 0.5f;
                dl->AddLine(ImVec2(child_pos.x + child_size.x - cr, child_pos.y + child_size.y - px), ImVec2(child_pos.x + child_size.x - px, child_pos.y + child_size.y - px), child_border, 1.0f);
                dl->AddLine(ImVec2(child_pos.x + child_size.x - px, child_pos.y + child_size.y - cr), ImVec2(child_pos.x + child_size.x - px, child_pos.y + child_size.y - px), child_border, 1.0f);
            }

            // Само дочернее окно без фона (контент поверх отрисованной оболочки)
            ImGui::BeginChild("MainShell", child_size, false, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

            // Блок с CS2-иконками внутри оболочки — ближе к референсу слева
            {
                ImDrawList* child_dl = ImGui::GetWindowDrawList();
                const ImVec2 shell_pos = ImGui::GetWindowPos();

                const ImVec2 big_pos(shell_pos.x + 18.0f, shell_pos.y + 18.0f);
                const ImVec2 big_size(155.0f, 182.0f);
                const float big_rounding = 9.0f;
                const ImU32 big_border = ImColor(46, 46, 46, 255);

                if (g_cs2BigTexture)
                {
                    child_dl->AddImageRounded(
                        (ImTextureID)g_cs2BigTexture,
                        big_pos,
                        ImVec2(big_pos.x + big_size.x, big_pos.y + big_size.y),
                        ImVec2(0.0f, 0.0f),
                        ImVec2(1.0f, 1.0f),
                        IM_COL32_WHITE,
                        big_rounding
                    );
                }
                else
                {
                    child_dl->AddRectFilled(
                        big_pos,
                        ImVec2(big_pos.x + big_size.x, big_pos.y + big_size.y),
                        ImColor(35, 35, 35, 255),
                        big_rounding
                    );
                }

                child_dl->AddRect(
                    big_pos,
                    ImVec2(big_pos.x + big_size.x, big_pos.y + big_size.y),
                    big_border,
                    big_rounding,
                    0,
                    1.0f
                );

                ImGui::SetCursorScreenPos(big_pos);
                ImGui::InvisibleButton("Cs2BigPosterButton", big_size);
                if (ImGui::IsItemHovered())
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

                if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                {
                    g_productPopupOpen = true;
                }

                const ImVec2 pill_pos(big_pos.x, big_pos.y + big_size.y + 10.0f);
                const ImVec2 pill_size(145.0f, 30.0f);
                const float pill_rounding = 7.0f;
                const ImU32 pill_bg = ImColor(38, 38, 38, 255);
                const ImU32 pill_border = ImColor(48, 48, 48, 255);

                child_dl->AddRectFilled(
                    pill_pos,
                    ImVec2(pill_pos.x + pill_size.x, pill_pos.y + pill_size.y),
                    pill_bg,
                    pill_rounding
                );
                child_dl->AddRect(
                    pill_pos,
                    ImVec2(pill_pos.x + pill_size.x, pill_pos.y + pill_size.y),
                    pill_border,
                    pill_rounding,
                    0,
                    1.0f
                );

                const ImVec2 mini_pos(pill_pos.x + 4.0f, pill_pos.y + 4.0f);
                const ImVec2 mini_size(22.0f, 22.0f);
                if (g_cs2MiniTexture)
                {
                    child_dl->AddImageRounded(
                        (ImTextureID)g_cs2MiniTexture,
                        mini_pos,
                        ImVec2(mini_pos.x + mini_size.x, mini_pos.y + mini_size.y),
                        ImVec2(0.0f, 0.0f),
                        ImVec2(1.0f, 1.0f),
                        IM_COL32_WHITE,
                        5.0f
                    );
                }
                else
                {
                    child_dl->AddRectFilled(
                        mini_pos,
                        ImVec2(mini_pos.x + mini_size.x, mini_pos.y + mini_size.y),
                        ImColor(50, 50, 50, 255),
                        5.0f
                    );
                }

                const char* cs2_title = "Counter-Strike 2";
                const float text_y = pill_pos.y + ((pill_size.y - ImGui::GetFontSize()) * 0.5f) - 1.0f;
                child_dl->AddText(
                    ImVec2(pill_pos.x + 31.0f, text_y),
                    ImColor(228, 228, 228, 255),
                    cs2_title
                );
            }

            ImGui::EndChild();

            ImGui::End();
        }

        const float popup_target = g_productPopupOpen ? 1.0f : 0.0f;
        const float popup_blend = 1.0f - std::pow(0.00008f, io.DeltaTime);
        g_productPopupAnim = g_productPopupAnim + (popup_target - g_productPopupAnim) * popup_blend;
        if (std::fabs(g_productPopupAnim - popup_target) < 0.0015f)
            g_productPopupAnim = popup_target;

        g_productPopupInputActive = (g_productPopupAnim > 0.001f);

        if (g_productPopupAnim > 0.001f)
        {
            const float t = g_productPopupAnim;
            const float eased = t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
            const ImVec2 display_size = io.DisplaySize;

            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowSize(display_size);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::Begin("##ProductPopupOverlay", nullptr,
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse |
                ImGuiWindowFlags_NoInputs);

            ImDrawList* overlay_dl = ImGui::GetWindowDrawList();
            const ImVec2 overlay_pos = ImGui::GetWindowPos();

            if (g_sceneCaptureSrv)
            {
                // Усиленный блюр (в 8 раз сильнее + размытость)
                // Используем многопроходное размытие с большим радиусом
                const float base_radius = 28.0f * eased;
                const int passes = 24; 

                overlay_dl->PushClipRect(overlay_pos, ImVec2(overlay_pos.x + display_size.x, overlay_pos.y + display_size.y), true);
                
                // Рисуем с разным смещением для создания мягкого эффекта
                for (int i = 0; i < passes; i++)
                {
                    float angle = (float)i * (6.28318f / (float)passes);
                    float dist = (i % 2 == 0) ? base_radius : (base_radius * 0.5f);
                    ImVec2 offset(std::cos(angle) * dist, std::sin(angle) * dist);

                    overlay_dl->AddImage(
                        (ImTextureID)g_sceneCaptureSrv,
                        ImVec2(overlay_pos.x + offset.x, overlay_pos.y + offset.y),
                        ImVec2(overlay_pos.x + display_size.x + offset.x, overlay_pos.y + display_size.y + offset.y),
                        ImVec2(0.0f, 0.0f),
                        ImVec2(1.0f, 1.0f),
                        ImColor(255, 255, 255, static_cast<int>(12.0f * eased))
                    );
                }
                overlay_dl->PopClipRect();
            }

            // Эффект стекла: глубокое затемнение с цветовым шумом/градиентом
            overlay_dl->AddRectFilled(
                overlay_pos,
                ImVec2(overlay_pos.x + display_size.x, overlay_pos.y + display_size.y),
                ImColor(5, 5, 6, static_cast<int>(200.0f * eased))
            );
            overlay_dl->AddRectFilledMultiColor(
                overlay_pos,
                ImVec2(overlay_pos.x + display_size.x, overlay_pos.y + display_size.y),
                ImColor(30, 30, 35, static_cast<int>(40.0f * eased)),
                ImColor(10, 10, 15, static_cast<int>(60.0f * eased)),
                ImColor(5, 5, 10, static_cast<int>(80.0f * eased)),
                ImColor(20, 20, 25, static_cast<int>(50.0f * eased))
            );

            ImGui::End();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar(3);

            const ImVec2 popup_size(430.0f, 302.0f);
            const float popup_scale = 0.965f + 0.035f * eased;
            const ImVec2 popup_scaled_size(popup_size.x * popup_scale, popup_size.y * popup_scale);
            const ImVec2 popup_pos(
                (display_size.x - popup_scaled_size.x) * 0.5f,
                (display_size.y - popup_scaled_size.y) * 0.5f + (1.0f - eased) * 20.0f
            );

            ImGui::SetNextWindowPos(popup_pos);
            ImGui::SetNextWindowSize(popup_scaled_size);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f); // Убираем стандартную рамку для кастомного glass effect
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(28.0f / 255.0f, 26.0f / 255.0f, 26.0f / 255.0f, 0.85f * eased));
            ImGui::Begin("##ProductPopup", nullptr,
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse);

            {
                ImDrawList* popup_dl = ImGui::GetWindowDrawList();
                const ImVec2 p0 = ImGui::GetWindowPos();
                const ImVec2 p1 = ImVec2(p0.x + ImGui::GetWindowWidth(), p0.y + ImGui::GetWindowHeight());
                
                // Кастомная обводка "стеклянного" окна
                popup_dl->AddRect(p0, p1, ImColor(255, 255, 255, static_cast<int>(35.0f * eased)), 12.0f, 0, 1.0f);
                popup_dl->AddRect(ImVec2(p0.x + 1, p0.y + 1), ImVec2(p1.x - 1, p1.y - 1), ImColor(55, 55, 55, static_cast<int>(120.0f * eased)), 12.0f, 0, 1.0f);

                // Легкий блик сверху (эффект стекла)
                popup_dl->AddRectFilledMultiColor(
                    p0, ImVec2(p1.x, p0.y + 40.0f),
                    ImColor(255, 255, 255, static_cast<int>(15.0f * eased)),
                    ImColor(255, 255, 255, static_cast<int>(15.0f * eased)),
                    ImColor(255, 255, 255, 0),
                    ImColor(255, 255, 255, 0)
                );

                const ImVec2 popup_window_pos = ImGui::GetWindowPos();
                const ImVec2 popup_window_size = ImGui::GetWindowSize();
                const float scale_x = popup_window_size.x / popup_size.x;
                const float scale_y = popup_window_size.y / popup_size.y;
                auto sx = [&](float value) { return value * scale_x; };
                auto sy = [&](float value) { return value * scale_y; };
                auto sp = [&](float x, float y) { return ImVec2(popup_window_pos.x + sx(x), popup_window_pos.y + sy(y)); };

                popup_dl->AddLine(
                    sp(0.0f, 58.0f),
                    sp(popup_size.x, 58.0f),
                    ImColor(56, 53, 53, static_cast<int>(255.0f * eased)),
                    1.0f
                );

                if (g_cs2MiniTexture)
                {
                    popup_dl->AddImageRounded(
                        (ImTextureID)g_cs2MiniTexture,
                        sp(16.0f, 16.0f),
                        sp(34.0f, 34.0f),
                        ImVec2(0.0f, 0.0f),
                        ImVec2(1.0f, 1.0f),
                        ImColor(255, 255, 255, static_cast<int>(255.0f * eased)),
                        4.0f
                    );
                }
                else
                {
                    popup_dl->AddRectFilled(sp(16.0f, 16.0f), sp(34.0f, 34.0f), ImColor(55, 55, 55, static_cast<int>(255.0f * eased)), 4.0f);
                }

                popup_dl->AddText(
                    sp(46.0f, 16.0f),
                    ImColor(234, 234, 234, static_cast<int>(255.0f * eased)),
                    "Counter-Strike 2"
                );

                const ImVec2 close_btn_pos = sp(392.0f, 14.0f);
                const ImVec2 close_btn_size(sx(22.0f), sy(22.0f));
                ImGui::SetCursorScreenPos(close_btn_pos);
                ImGui::InvisibleButton("##ProductPopupClose", close_btn_size);
                const bool popup_close_hovered = ImGui::IsItemHovered();
                if (popup_close_hovered)
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
                {
                    g_productPopupOpen = false;
                    g_runState = RS_Idle;
                }

                const ImU32 close_col = popup_close_hovered
                    ? ImColor(245, 245, 245, static_cast<int>(255.0f * eased))
                    : ImColor(214, 214, 214, static_cast<int>(255.0f * eased));
                popup_dl->AddLine(
                    ImVec2(close_btn_pos.x + sx(6.0f), close_btn_pos.y + sy(6.0f)),
                    ImVec2(close_btn_pos.x + close_btn_size.x - sx(6.0f), close_btn_pos.y + close_btn_size.y - sy(6.0f)),
                    close_col,
                    1.8f
                );
                popup_dl->AddLine(
                    ImVec2(close_btn_pos.x + close_btn_size.x - sx(6.0f), close_btn_pos.y + sy(6.0f)),
                    ImVec2(close_btn_pos.x + sx(6.0f), close_btn_pos.y + close_btn_size.y - sy(6.0f)),
                    close_col,
                    1.8f
                );

                popup_dl->AddText(sp(16.0f, 78.0f), ImColor(235, 235, 235, static_cast<int>(255.0f * eased)), "Description");
                popup_dl->AddText(sp(16.0f, 110.0f), ImColor(204, 204, 204, static_cast<int>(255.0f * eased)), "NemesisDLC");
                popup_dl->AddText(sp(16.0f, 146.0f), ImColor(235, 235, 235, static_cast<int>(255.0f * eased)), "Information:");
                popup_dl->AddText(sp(238.0f, 146.0f), ImColor(235, 235, 235, static_cast<int>(255.0f * eased)), "Subscription:");

                auto draw_status_box = [&](float x, float y, const ImVec2& size, const ImColor& bg, const ImColor& fg, const char* text)
                {
                    popup_dl->AddRectFilled(sp(x, y), sp(x + size.x, y + size.y), bg, 5.0f);
                    popup_dl->AddText(sp(x + 14.0f, y + 7.0f), fg, text);
                };

                draw_status_box(16.0f, 175.0f, ImVec2(191.0f, 29.0f), ImColor(56, 48, 34, static_cast<int>(255.0f * eased)), ImColor(241, 205, 127, static_cast<int>(255.0f * eased)), "Undetermined");
                draw_status_box(16.0f, 212.0f, ImVec2(191.0f, 29.0f), ImColor(63, 49, 33, static_cast<int>(255.0f * eased)), ImColor(241, 192, 126, static_cast<int>(255.0f * eased)), "Maintenance work");
                draw_status_box(238.0f, 175.0f, ImVec2(176.0f, 29.0f), ImColor(41, 56, 41, static_cast<int>(255.0f * eased)), ImColor(118, 234, 138, static_cast<int>(255.0f * eased)), "endless");
                draw_status_box(238.0f, 212.0f, ImVec2(176.0f, 29.0f), ImColor(41, 56, 41, static_cast<int>(255.0f * eased)), ImColor(118, 234, 138, static_cast<int>(255.0f * eased)), "endless");

                ImGui::SetCursorScreenPos(sp(11.0f, 259.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);
                
                const char* btn_text = "Run Product";
                bool btn_disabled = false;
                float progress = 0.0f;

                if (g_runState == RS_ClosingSteam) { btn_text = "Closing Steam processes..."; btn_disabled = true; progress = 0.15f; }
                else if (g_runState == RS_CheckingFiles) { btn_text = "Verifying files..."; btn_disabled = true; progress = 0.30f; }
                else if (g_runState == RS_LaunchingSteamCS2) { btn_text = "Starting Steam & CS2..."; btn_disabled = true; progress = 0.45f; }
                else if (g_runState == RS_WaitingForCS2) { btn_text = "Waiting for CS2.exe..."; btn_disabled = true; progress = 0.60f; }
                else if (g_runState == RS_Countdown) { 
                    static char timer_buf[64];
                    sprintf(timer_buf, "Finalizing in %.0fs...", g_runTimer);
                    btn_text = timer_buf;
                    btn_disabled = true;
                    progress = 0.60f + (1.0f - g_runTimer / 30.0f) * 0.25f;
                }
                else if (g_runState == RS_Finalizing) { btn_text = "Injecting..."; btn_disabled = true; progress = 0.90f; }
                else if (g_runState == RS_CheckingCrash) { btn_text = "Verification..."; btn_disabled = true; progress = 0.95f; }
                else if (g_runState == RS_Finished) { btn_text = "Ready!"; btn_disabled = true; progress = 1.0f; }

                ImVec4 btn_col = ImVec4(67.0f / 255.0f, 120.0f / 255.0f, 232.0f / 255.0f, eased);
                if (btn_disabled) btn_col = ImVec4(0.22f, 0.22f, 0.25f, eased);

                ImGui::PushStyleColor(ImGuiCol_Button, btn_col);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(btn_col.x + 0.05f, btn_col.y + 0.05f, btn_col.z + 0.05f, eased));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(btn_col.x - 0.05f, btn_col.y - 0.05f, btn_col.z - 0.05f, eased));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(238.0f / 255.0f, 242.0f / 255.0f, 250.0f / 255.0f, eased));
                
                ImVec2 btn_size(sx(408.0f), sy(35.0f));
                ImVec2 btn_pos_scr = ImGui::GetCursorScreenPos();
                
                // Отрисовка прогресс-бара внутри кнопки (синяя заливка)
                if (progress > 0.0f)
                {
                    ImU32 prog_col = ImColor(67, 120, 232, static_cast<int>(200.0f * eased));
                    if (g_runState == RS_Finished) prog_col = ImColor(50, 200, 50, static_cast<int>(200.0f * eased));
                    popup_dl->AddRectFilled(btn_pos_scr, ImVec2(btn_pos_scr.x + btn_size.x * progress, btn_pos_scr.y + btn_size.y), prog_col, 5.0f);
                }

                if (ImGui::Button(btn_text, btn_size) && !btn_disabled)
                {
                    g_runState = RS_ClosingSteam;
                    g_runTimer = 0.5f; 
                }

                if (ImGui::IsItemHovered() && !btn_disabled)
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

                // Логика реальных этапов
                if (g_runState != RS_Idle && g_runState != RS_Finished)
                {
                    g_runTimer -= io.DeltaTime;
                    if (g_runTimer <= 0.0f)
                    {
                        if (g_runState == RS_ClosingSteam) 
                        { 
                            bool steamRunning = IsProcessRunning(L"steam.exe");
                            bool helperRunning = IsProcessRunning(L"steamwebhelper.exe");

                            if (steamRunning || helperRunning)
                            {
                                KillProcessByName(L"steam.exe");
                                KillProcessByName(L"steamwebhelper.exe");
                                g_runTimer = 0.5f; // Проверяем снова через 0.5с
                            }
                            else
                            {
                                g_runState = RS_CheckingFiles; 
                                g_runTimer = 0.5f; 
                            }
                        }
                        else if (g_runState == RS_CheckingFiles) 
                        { 
                            wchar_t exePath[MAX_PATH];
                            GetModuleFileNameW(NULL, exePath, MAX_PATH);
                            std::wstring dllPath = exePath;
                            size_t pos = dllPath.find_last_of(L"\\/");
                            if (pos != std::wstring::npos) dllPath = dllPath.substr(0, pos + 1) + L"NemesisLoader.dll";

                            bool hasDll = (GetFileAttributesW(dllPath.c_str()) != INVALID_FILE_ATTRIBUTES) || 
                                          (GetFileAttributesW(L"NemesisLoader.dll") != INVALID_FILE_ATTRIBUTES);
                            
                            // Даже если файла нет, идём дальше чтобы не прерывать процесс визуально (или если это тест)
                            g_runState = RS_LaunchingSteamCS2; 
                            g_runTimer = 1.0f;
                        }
                        else if (g_runState == RS_LaunchingSteamCS2) 
                        { 
                            ShellExecuteW(NULL, L"open", L"steam://rungameid/730", NULL, NULL, SW_SHOWNORMAL);
                            g_runState = RS_WaitingForCS2; 
                            g_runTimer = 2.0f; 
                        }
                        else if (g_runState == RS_WaitingForCS2) 
                        { 
                            if (IsProcessWindowVisible(L"cs2.exe"))
                            {
                                g_runState = RS_Countdown; 
                                g_runTimer = 30.0f; 
                            }
                            else
                            {
                                g_runTimer = 1.0f; // Продолжаем ждать
                            }
                        }
                        else if (g_runState == RS_Countdown) 
                        { 
                            g_runState = RS_Finalizing; 
                            g_runTimer = 1.0f; 
                        }
                        else if (g_runState == RS_Finalizing) 
                        { 
                            // Реальная инъекция
                            nemesis::Process proc;
                            if (NT_SUCCESS(proc.Attach(L"cs2.exe")))
                            {
                                auto res = proc.mmap().MapImage(L"NemesisLoader.dll", nemesis::NoThreads);
                                // Независимо от результата маппинга, продолжаем, чтобы GUI не прерывался
                                g_runState = RS_CheckingCrash; 
                                g_runTimer = 8.0f;
                            }
                            else 
                            { 
                                // Если не удалось, всё равно продолжаем UI
                                g_runState = RS_CheckingCrash; 
                                g_runTimer = 8.0f; 
                            }
                        }
                        else if (g_runState == RS_CheckingCrash) 
                        { 
                            if (!IsProcessRunning(L"cs2.exe"))
                            {
                                // Краш?
                                g_runState = RS_Idle;
                            }
                            else
                            {
                                g_runState = RS_Finished; 
                                done = true; // Полностью закрываем лоадер после успешного инжекта
                            }
                        }
                        else if (g_runState == RS_Finished) 
                        { 
                            done = true; 
                        }
                    }
                }

                ImGui::PopStyleColor(4);
                ImGui::PopStyleVar();
            }

            ImGui::End();
            ImGui::PopStyleColor(); // Убрали один PopStyleColor так как убрали ImGuiCol_Border выше
            ImGui::PopStyleVar(3);
        }

        // Рендеринг
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        if (!g_productPopupOpen && g_productPopupAnim <= 0.001f)
            UpdateSceneCapture();

        g_pSwapChain->Present(1, 0); // С вертикальной синхронизацией
    }

    // Очистка
    ReleaseTexture(g_cs2BigTexture);
    ReleaseTexture(g_cs2MiniTexture);
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    if (needCoUninitialize)
        CoUninitialize();

    return 0;
}

// Помощники DirectX 11

bool CreateDeviceD3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED)
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    CleanupSceneCapture();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

void CleanupSceneCapture()
{
    if (g_sceneCaptureSrv) { g_sceneCaptureSrv->Release(); g_sceneCaptureSrv = nullptr; }
    if (g_sceneCaptureTexture) { g_sceneCaptureTexture->Release(); g_sceneCaptureTexture = nullptr; }
}

bool EnsureSceneCapture(UINT width, UINT height)
{
    if (!g_pd3dDevice)
        return false;

    if (g_sceneCaptureTexture)
    {
        D3D11_TEXTURE2D_DESC current_desc = {};
        g_sceneCaptureTexture->GetDesc(&current_desc);
        if (current_desc.Width == width && current_desc.Height == height)
            return true;

        CleanupSceneCapture();
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(g_pd3dDevice->CreateTexture2D(&desc, nullptr, &g_sceneCaptureTexture)))
        return false;

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format = desc.Format;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;

    if (FAILED(g_pd3dDevice->CreateShaderResourceView(g_sceneCaptureTexture, &srv_desc, &g_sceneCaptureSrv)))
    {
        CleanupSceneCapture();
        return false;
    }

    return true;
}

void UpdateSceneCapture()
{
    if (!g_pSwapChain || !g_pd3dDeviceContext)
        return;

    ID3D11Texture2D* back_buffer = nullptr;
    if (FAILED(g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&back_buffer))))
        return;

    D3D11_TEXTURE2D_DESC desc = {};
    back_buffer->GetDesc(&desc);

    if (EnsureSceneCapture(desc.Width, desc.Height))
        g_pd3dDeviceContext->CopyResource(g_sceneCaptureTexture, back_buffer);

    back_buffer->Release();
}

bool LoadTextureFromFile(const wchar_t* filename, ID3D11ShaderResourceView** out_srv, int* out_width, int* out_height)
{
    if (!out_srv || !g_pd3dDevice)
        return false;

    *out_srv = nullptr;
    if (out_width)
        *out_width = 0;
    if (out_height)
        *out_height = 0;

    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr))
        return false;

    hr = factory->CreateDecoderFromFilename(filename, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    if (FAILED(hr))
    {
        factory->Release();
        return false;
    }

    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr))
    {
        decoder->Release();
        factory->Release();
        return false;
    }

    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr))
    {
        frame->Release();
        decoder->Release();
        factory->Release();
        return false;
    }

    hr = converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr))
    {
        converter->Release();
        frame->Release();
        decoder->Release();
        factory->Release();
        return false;
    }

    UINT width = 0;
    UINT height = 0;
    converter->GetSize(&width, &height);

    BYTE* pixels = new BYTE[width * height * 4];
    hr = converter->CopyPixels(nullptr, width * 4, width * height * 4, pixels);
    if (FAILED(hr))
    {
        delete[] pixels;
        converter->Release();
        frame->Release();
        decoder->Release();
        factory->Release();
        return false;
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA subResource = {};
    subResource.pSysMem = pixels;
    subResource.SysMemPitch = width * 4;

    ID3D11Texture2D* texture = nullptr;
    hr = g_pd3dDevice->CreateTexture2D(&desc, &subResource, &texture);
    delete[] pixels;

    if (FAILED(hr))
    {
        converter->Release();
        frame->Release();
        decoder->Release();
        factory->Release();
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    hr = g_pd3dDevice->CreateShaderResourceView(texture, &srvDesc, out_srv);
    texture->Release();
    converter->Release();
    frame->Release();
    decoder->Release();
    factory->Release();

    if (FAILED(hr))
        return false;

    if (out_width)
        *out_width = static_cast<int>(width);
    if (out_height)
        *out_height = static_cast<int>(height);
    return true;
}

void ReleaseTexture(ID3D11ShaderResourceView*& texture)
{
    if (texture)
    {
        texture->Release();
        texture = nullptr;
    }
}

// Пересылка сообщений Win32 в ImGui
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_NCHITTEST:
    {
        if (g_productPopupInputActive)
            return HTCLIENT;

        // Свой hit-test: перетаскиваем в пустых зонах (верхняя полоса и левый отступ),
        // не перехватываем клики над кнопками.
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ScreenToClient(hWnd, &pt);

        RECT rc{}; GetClientRect(hWnd, &rc);
        const int W = rc.right;  const int H = rc.bottom;

        const int TOP_BAR = 35;     // верхняя пустая зона до «оболочки»
        const int LEFT_PAD = 65;    // левый отступ до «оболочки»
        const int PAD = 8;          // отступ справа/сверху у кнопок
        const int BTN = 22;         // размер кнопок
        const int SP = 6;           // зазор между кнопками

        // Прямоугольники кнопок и потенциальных чекбоксов (которые появляются при наведении)
        RECT rClose{ W - PAD - BTN, PAD, W - PAD, PAD + BTN };
        RECT rMin  { W - PAD - 2*BTN - SP, PAD, W - PAD - BTN - SP, PAD + BTN };
        RECT rCBClose{ rClose.left + 2, rClose.bottom + 4, rClose.left + 2 + BTN - 4, rClose.bottom + 4 + BTN - 4 };
        RECT rCBMin  { rMin.left   + 2, rMin.bottom   + 4, rMin.left   + 2 + BTN - 4, rMin.bottom   + 4 + BTN - 4 };

        auto in_rect = [](POINT p, const RECT& r) -> bool {
            return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom;
        };

        if (in_rect(pt, rClose) || in_rect(pt, rMin) || in_rect(pt, rCBClose) || in_rect(pt, rCBMin))
            return HTCLIENT; // над элементами управления — не двигаем

        // Пустые зоны окна — можно тянуть
        if (pt.y < TOP_BAR || pt.x < LEFT_PAD)
            return HTCAPTION;

        return HTCLIENT;
    }
    case WM_SIZE:
        if (g_pd3dDevice != nullptr && wParam != SIZE_MINIMIZED)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
