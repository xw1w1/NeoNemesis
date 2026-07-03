#include <windows.h>
#include <windowsx.h> // для макросов GET_X_LPARAM/GET_Y_LPARAM
#include <shlobj.h>   // для IsUserAnAdmin
#include <d3d11.h>
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
#include "imgui_extensions.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"

#include "file_utils.h"
#include "layout_utils.h"
#include "process_helper.h"
#include "steam\steam_helper.h"
#include "steam\avatar_loader.h"
#include "system_info.h"

#include "api\loader_api.h"
#include "subscription\product.h"

#include "product_registry.h"

static HWND                      g_Hwnd = nullptr;
static SystemInfo                g_systemInfo;
static bool                      g_systemInfoLoaded = false;
// Данные для DirectX 11
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

// Пользовательский шрифт для заголовка
static ImFont* g_titleFont = nullptr;
static ID3D11ShaderResourceView* g_titleIcon = nullptr;

static ImU32                     g_accentColor = ImColor(230, 25, 52, 255);
static ImU32                     g_accentColorSub = ImColor(250, 82, 101, 255);
static ImVec4                    g_fillColorMain = ImColor(23, 23, 23, 255);
static ImVec4                    g_fillColorSub = ImColor(31, 30, 30, 255);

static ID3D11ShaderResourceView* g_libraryIcon = nullptr;
static ID3D11ShaderResourceView* g_profileIcon = nullptr;
static ID3D11ShaderResourceView* g_settingsIcon = nullptr;
static ID3D11ShaderResourceView* g_accountsIcon = nullptr;

static ID3D11Texture2D*          g_sceneCaptureTexture = nullptr;
static ID3D11ShaderResourceView* g_sceneCaptureSrv = nullptr;

static int                       g_titleRegionHeight = 54;
static int                       g_titleColliderHeight = 35;

static int                       g_frameOffsetX = 65;
static int                       g_frameOffsetY = 48;
static int                       g_controlPanelWidth = 115;

static float                     g_productCardWidth = 155.0f;
static float                     g_productCardRounding = 10.0f;

static bool                      g_productPopupOpen = false;
static float                     g_productPopupAnim = 0.0f;
static bool                      g_productPopupInputActive = false;
static float                     g_libraryScrollSpeedMultiplier = 0.15f;

static bool                      g_isAppDone = false;
static int                       g_currentPage = 0; // текущая страница. 0 - библиотека 1 - профиль

// Время, которое программа должна ждать после захвата процесса
// Это время появляется в таймере до инжекта, давая кске больше времени открыться
static int                       g_rsCountdownTime = 55.0f;

static Product*                  p_currentProduct = nullptr;

enum eRunState {
    RS_Idle,
    RS_CheckingSteamState,
    RS_ClosingSteam,
    RS_CheckingFiles,
    RS_LaunchingSteam,
    RS_WaitingForSteam,
    RS_LaunchingSteamGame,
    RS_WaitingForGame,
    RS_Countdown,
    RS_Finalizing,
    RS_CheckingCrash,
    RS_Finished
};

static eRunState g_runState = RS_Idle;
static float     g_runTimer = 0.0f;

static std::vector<SteamAccount> g_steamAccounts;
static SteamAccount*             g_currentAccount = nullptr;
static bool                      g_accountsLoaded = false;

// Прототипы функций
void CreateResources();

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();

// Рисует кастомные кнопки на окне.
void DrawWindowControls();

void DrawWindowBackgroundBody();
void DrawControlPanel(ImVec2 avail, float lPw);
void DrawContentPaneBody(ImVec2 avail, float lPw);

void DrawProductCard(Product* product, const ImVec2& card_size);
bool DrawAccountCard(const SteamAccount& acc, bool is_current, float width);

void DrawLibraryPage();
void DrawProfilePage();
void DrawAccountsPage();
void DrawSettingsPage();

void CleanupSceneCapture();
bool EnsureSceneCapture(UINT width, UINT height);
void UpdateSceneCapture();

void RefreshSteamAccounts();

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Хелперы для ImGui
inline ImVec2  operator+(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y); }
inline ImVec2  operator-(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y); }
inline ImVec2  operator*(const ImVec2& lhs, const float rhs) { return ImVec2(lhs.x * rhs, lhs.y * rhs); }
inline ImVec2& operator+=(ImVec2& lhs, const ImVec2& rhs) { lhs.x += rhs.x; lhs.y += rhs.y; return lhs; }
inline ImVec2& operator-=(ImVec2& lhs, const ImVec2& rhs) { lhs.x -= rhs.x; lhs.y -= rhs.y; return lhs; }

int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow)
{
    // Очередь проверок
    if (!IsUserAnAdmin())
    {
        MessageBoxW(NULL, L"Administrator rights are required to run this application.", L"Nemesis Loader", MB_ICONERROR);
        return 1;
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
    ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"Nemesis Loader", nullptr };
    ::RegisterClassExW(&wc);

    // Создаем окно без рамки и кнопок (WS_POPUP) размером 685x450
    const int kWinW = 720;
    const int kWinH = 480;

    // Центрирование окна
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int winX = (screenW - kWinW) / 2;
    int winY = (screenH - kWinH) / 2;
    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    g_Hwnd = ::CreateWindowW(wc.lpszClassName, L"Nemesis Loader", WS_POPUP, winX, winY, (int)(kWinW * main_scale), (int)(kWinH * main_scale), nullptr, nullptr, wc.hInstance, nullptr);

    // Формируем регион окна: 4 скругленных угла (TL/TR/BL/BR)
    // Радиус делаем немного меньше (10px), чтобы углы были поменьше и визуально мягче
    const int kRadius = 10;          // радиус скругления в пикселях
    const int kDiam = kRadius * 2; // диаметр для CreateRoundRectRgn

    HRGN rRoundAll = CreateRoundRectRgn(0, 0, kWinW + 1, kWinH + 1, kDiam, kDiam);
    SetWindowRgn(g_Hwnd, rRoundAll, TRUE); // система возьмёт владение rRoundAll

    // Инициализация Direct3D
    if (!CreateDeviceD3D(g_Hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Показ окна
    ::ShowWindow(g_Hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(g_Hwnd);

    // Инициализация ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Настройка стиля
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    // Углы делаем немного меньше и «гладкими»
    style.WindowRounding = 10.0f;
    style.ChildRounding = 10.0f;
    style.WindowBorderSize = 1.0f; // 1px обводка вокруг GUI
    style.ChildBorderSize = 0.0f;
    style.AntiAliasedFill = true;
    style.AntiAliasedLines = true;

    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    // Чуть-чуть темнее серая обводка
    style.Colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);
    // Делаем фон окна непрозрачным и соответствующим clear_color
    style.Colors[ImGuiCol_WindowBg] = g_fillColorMain;

    // Настройка бэкендов
    ImGui_ImplWin32_Init(g_Hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Инициализация ресурсов
    CreateResources();

    // Цвет фона: 31, 30, 30
    ImVec4 clear_color = ImVec4(31.0f / 255.0f, 30.0f / 255.0f, 30.0f / 255.0f, 1.00f);

    // Главный цикл
    g_isAppDone = false;
    while (!g_isAppDone)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                g_isAppDone = true;
        }
        if (g_isAppDone)
            break;

        // Запуск кадра ImGui
        // обработка стейта аватарок
        AvatarLoader::ProcessCompletedTasks();
        ProductRegistry::ProcessCompletedTasks();
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Отрисовка кастомного окна
        {
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(kWinW, kWinH));

            ImGuiWindowFlags main_flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
            if (g_productPopupInputActive) main_flags |= ImGuiWindowFlags_NoInputs; // Отключаем клики по главному окну, чтобы они не перехватывали фокус

            ImGui::Begin("Nemesis Loader", nullptr, main_flags);

            DrawWindowBackgroundBody();

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
                ImGuiWindowFlags_NoScrollWithMouse
            );

            // Я убрал NoInputs из ^ флагов окна и добавил:
            ImGui::InvisibleButton("overlay", display_size);

            if (ImGui::IsItemClicked())
            {
                g_productPopupOpen = false;
                g_runState = RS_Idle;
            }

            // Невидимую кнопку на весь размер окна. При клике на нее
            // карточка (g_productPopupOpen) становится не открытой,
            // g_runState становится idle чтобы аутист не пытался снова захватить инпуты

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

                if (p_currentProduct->GetCurrentIcon())
                {
                    popup_dl->AddImageRounded(
                        (ImTextureID)p_currentProduct->GetCurrentIcon(),
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
                    p_currentProduct->GetTitle().c_str()
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

                if (g_runState == RS_CheckingSteamState) { btn_text = "Checking Steam state..."; btn_disabled = true; progress = 0.05f; }
                else if (g_runState == RS_ClosingSteam) { btn_text = "Closing Steam..."; btn_disabled = true; progress = 0.15f; }
                else if (g_runState == RS_CheckingFiles) { btn_text = "Verifying files..."; btn_disabled = true; progress = 0.25f; }
                else if (g_runState == RS_LaunchingSteam) { btn_text = "Launching Steam..."; btn_disabled = true; progress = 0.35f; }
                else if (g_runState == RS_WaitingForSteam) { btn_text = "Waiting for Steam..."; btn_disabled = true; progress = 0.45f; }
                else if (g_runState == RS_LaunchingSteamGame) { btn_text = "Starting game..."; btn_disabled = true; progress = 0.55f; }
                else if (g_runState == RS_WaitingForGame) { btn_text = "Waiting for game to launch..."; btn_disabled = true; progress = 0.60f; }
                else if (g_runState == RS_Countdown) {
                    static char timer_buf[64];
                    sprintf(timer_buf, "Finalizing in %.0fs...", g_runTimer);
                    btn_text = timer_buf;
                    btn_disabled = true;
                    progress = 0.60f + (1.0f - g_runTimer / g_rsCountdownTime) * 0.25f;
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
                {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }

                // Логика реальных этапов
                if (g_runState != RS_Idle && g_runState != RS_Finished)
                {
                    g_runTimer -= io.DeltaTime;
                    if (g_runTimer <= 0.0f)
                    {
                        if (g_runState == RS_CheckingSteamState)
                        {
                            bool needRestart = true;

                            if (SteamHelper::IsSteamReady() && g_currentAccount)
                            {
                                std::string current_user = SteamHelper::GetCurrentLoggedInUser();
                                if (!current_user.empty() && current_user == g_currentAccount->GetAccountName())
                                {
                                    // если стим уже запущен то не убиваем
                                    needRestart = false;
                                }
                            }

                            if (needRestart)
                            {
                                g_runState = RS_ClosingSteam;
                            }
                            else
                            {
                                // стим готов к запуску, значит можно лаунчить игру
                                g_runState = RS_CheckingFiles;
                            }
                            g_runTimer = 0.3f;
                        }
                        else if (g_runState == RS_ClosingSteam)
                        {
                            bool steamRunning = IsProcessRunning(L"steam.exe");
                            bool helperRunning = IsProcessRunning(L"steamwebhelper.exe");

                            if (steamRunning || helperRunning)
                            {
                                // если тварь не закроется за 3 секунды, нужно
                                // закрыть стим принудительно
                                static bool tried_soft = false;
                                if (!tried_soft)
                                {
                                    SteamHelper::ShutdownSteam();
                                    tried_soft = true;
                                    g_runTimer = 3.0f;
                                }
                                else
                                {
                                    KillProcessByName(L"steam.exe");
                                    KillProcessByName(L"steamwebhelper.exe");
                                    g_runTimer = 1.5f; // освобождение ресурсов винды
                                    tried_soft = false; // на будущее
                                }
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

                            if (SteamHelper::IsSteamReady())
                            {
                                g_runState = RS_LaunchingSteamGame;
                            }
                            else
                            {
                                g_runState = RS_LaunchingSteam;
                            }
                            g_runTimer = 0.5f;
                        }
                        else if (g_runState == RS_LaunchingSteam)
                        {
                            if (g_currentAccount)
                            {
                                SteamHelper::LaunchSteamAs(*g_currentAccount);
                            }
                            else
                            {
                                std::string steamPath = SteamHelper::GetSteamInstallPath();
                                if (!steamPath.empty())
                                {
                                    std::string cmd = steamPath + "\\steam.exe";
                                    ShellExecuteA(NULL, "open", cmd.c_str(), NULL, NULL, SW_SHOWNORMAL);
                                }
                            }
                            g_runState = RS_WaitingForSteam;
                            g_runTimer = 3.0f;
                        }
                        else if (g_runState == RS_WaitingForSteam)
                        {
                            static float total_wait = 0.0f;
                            total_wait += 1.0f;

                            if (SteamHelper::IsSteamReady())
                            {
                                g_runState = RS_LaunchingSteamGame;
                                g_runTimer = 1.5f;
                                total_wait = 0.0f;
                            }
                            else if (total_wait > 60.0f)
                            {
                                g_runState = RS_Idle;
                                total_wait = 0.0f;
                            }
                            else
                            {
                                g_runTimer = 1.0f;
                            }
                        }
                        else if (g_runState == RS_LaunchingSteamGame)
                        {
                            // Steam уже готов — запускаем игру через URL
                            auto product_path = std::wstring(L"steam://rungameid/") + std::to_wstring(p_currentProduct->GetSteamId());
                            ShellExecuteW(NULL, L"open", product_path.c_str(), NULL, NULL, SW_SHOWNORMAL);
                            g_runState = RS_WaitingForGame;
                            g_runTimer = 2.0f;
                        }
                        else if (g_runState == RS_WaitingForGame)
                        {
                            if (IsProcessWindowVisible(p_currentProduct->GetProcNameW().c_str()))
                            {
                                g_runState = RS_Countdown;
                                g_runTimer = g_rsCountdownTime;
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
                            if (NT_SUCCESS(proc.Attach(p_currentProduct->GetProcNameW().c_str())))
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
                            if (!IsProcessRunning(p_currentProduct->GetProcNameW().c_str()))
                            {
                                g_runState = RS_Idle;
                            }
                            else
                            {
                                g_runState = RS_Finished;
                                g_isAppDone = true;
                            }
                        }
                        else if (g_runState == RS_Finished)
                        {
                            g_isAppDone = true;
                        }
                        else {};
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

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    AvatarLoader::Shutdown();
    CleanupDeviceD3D();
    ::DestroyWindow(g_Hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    if (needCoUninitialize)
        CoUninitialize();

    return 0;
}

// Отрисовывает кнопки окна
void DrawWindowControls()
{
    const float pad = 8.0f;
    const float btn = 22.0f;
    ImVec2 ws = ImGui::GetWindowSize();
    ImVec2 wnd = ImGui::GetWindowPos();

    auto draw_icon_button = [&](const char* id,
        const ImVec2& local_pos,
        bool is_close,
        ImU32 col_symbol,
        ImU32 col_bar) -> bool
        {
            ImGui::SetCursorPos(local_pos);
            ImVec2 p = ImGui::GetCursorScreenPos();
            ImDrawList* dl = ImGui::GetWindowDrawList();

            bool pressed = ImGui::InvisibleButton(id, ImVec2(btn, btn));

            const float m = 6.0f;
            const float thick = 2.0f;

            if (is_close)
            {
                dl->AddLine(ImVec2(p.x + m, p.y + m), ImVec2(p.x + btn - m, p.y + btn - m), col_symbol, thick);
                dl->AddLine(ImVec2(p.x + btn - m, p.y + m), ImVec2(p.x + m, p.y + btn - m), col_symbol, thick);
            }
            else
            {
                float y = p.y + btn * 0.60f;
                dl->AddLine(ImVec2(p.x + m, y), ImVec2(p.x + btn - m, y), col_symbol, thick);
            }

            if (ImGui::IsItemHovered())
            {
                float w = 14.0f;
                float h = 2.0f;

                ImVec2 p0(wnd.x + local_pos.x + (btn - w) * 0.5f, wnd.y + local_pos.y + btn + 3.0f);
                ImVec2 p1(p0.x + w, p0.y + h);
                dl->AddRectFilled(p0, p1, col_bar, 1.0f);
            }

            return pressed;
        };

    const ImU32 col_symbol = ImColor(210, 214, 225, 255);

    ImVec2 pos_close(ws.x - pad - btn, pad);
    ImVec2 pos_min(ws.x - pad - 2.0f * btn - 6.0f, pad);

    if (draw_icon_button("##btn_close", pos_close, true, col_symbol, g_accentColor))
    {
        g_isAppDone = true;
    }

    if (draw_icon_button("##btn_min", pos_min, false, col_symbol, g_accentColor))
    {
        ShowWindow(g_Hwnd, SW_MINIMIZE);
    }
}

// Рисует само "тело" окна, все кроме заголовка
void DrawWindowBackgroundBody()
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImGui::SetCursorPos(ImVec2(0, 0));

    const ImVec2 window_size = ImGui::GetWindowSize();
    const ImVec2 avail(window_size.x, window_size.y);
    const ImVec2 avail_pane(window_size.x, window_size.y - g_titleColliderHeight);
    const ImVec2 child_size(avail.x, avail.y);

    ImGui::BeginChild(
        "MainShell", child_size, false,
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    );
    {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        const float leftPanelWidth = g_controlPanelWidth;

        DrawControlPanel(avail, leftPanelWidth);
        DrawWindowControls();
        DrawContentPaneBody(avail_pane, leftPanelWidth);
    }
    ImGui::EndChild();
}

// Рисует левую панель окна
void DrawControlPanel(ImVec2 avail, float lPw)
{
    ImDrawList*  dl = ImGui::GetWindowDrawList();
    const ImVec2 child_size(lPw, avail.y);
    const float  cr = ImGui::GetStyle().ChildRounding;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild(
        "LeftPanel", child_size, false,
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    );
    {
        ImGui::PopStyleVar();

        const float edge_padding = 12.0f;
        const float top_padding = 24.0f;
        const float icon_to_buttons_gap = 28.0f;
        const float button_gap = 6.0f;

        const float icon_size = 56;
        const float icon_pos_x = (lPw - icon_size) / 2.0f;
        ImGui::SetCursorPos(ImVec2(icon_pos_x, top_padding));
        ImGui::Image((ImTextureID)g_titleIcon, ImVec2(icon_size, icon_size));

        ImGui::Dummy(ImVec2(0.0f, icon_to_buttons_gap));

        float line_height = ImGui::GetTextLineHeight();
        float frame_padding = line_height / 2.0f;
        float top_icon_size = line_height * 1.25f;
        float bottom_icon_size = line_height;

        const ImVec2 top_isize(top_icon_size, top_icon_size);
        const ImVec2 bottom_isize(bottom_icon_size, bottom_icon_size);

        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, cr);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(frame_padding, frame_padding));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(35.0f / 255.0f, 35.0f / 255.0f, 35.0f / 255.0f, 1.00f));

        const float button_width = lPw - 2.0f * edge_padding;

        ImGui::SetCursorPosX(edge_padding);
        if (ImGuiExt::IconizedButton((ImTextureID)g_libraryIcon, "Library", top_isize,
            g_accentColor, g_accentColorSub, (g_currentPage == 0), ImGuiButtonFlags_None, 6.0f,
            ImVec2(button_width, 0.0f)))
        {
            g_currentPage = 0;
        }

        ImGui::Dummy(ImVec2(0.0f, button_gap));

        ImGui::SetCursorPosX(edge_padding);
        if (ImGuiExt::IconizedButton((ImTextureID)g_accountsIcon, "Steam", top_isize,
            g_accentColor, g_accentColorSub, (g_currentPage == 3), ImGuiButtonFlags_None, 6.0f,
            ImVec2(button_width, 0.0f)))
        {
            g_currentPage = 3;
        }

        float compact_content_h = ImMax(bottom_icon_size, ImGui::CalcTextSize("Settings").y);
        float compact_button_height = compact_content_h + 2.0f * frame_padding;

        float bottom_block_height = compact_button_height * 2.0f + button_gap;

        float cursor_y_current = ImGui::GetCursorPosY();
        float window_height = ImGui::GetWindowHeight();

        float bottom_padding = edge_padding * 2.0f;

        float bottom_block_y = window_height - bottom_padding - bottom_block_height;

        ImGui::SetCursorPosY(ImMax(cursor_y_current, bottom_block_y));

        ImGui::SetCursorPosX(edge_padding);
        if (ImGuiExt::IconizedButton((ImTextureID)g_profileIcon, "Profile", bottom_isize,
            g_accentColor, g_accentColorSub, (g_currentPage == 1), ImGuiButtonFlags_None, 6.0f,
            ImVec2(button_width, compact_button_height)))
        {
            g_currentPage = 1;
        }

        ImGui::Dummy(ImVec2(0.0f, button_gap));

        ImGui::SetCursorPosX(edge_padding);
        if (ImGuiExt::IconizedButton((ImTextureID)g_settingsIcon, "Settings", bottom_isize,
            g_accentColor, g_accentColorSub, (g_currentPage == 2), ImGuiButtonFlags_None, 6.0f,
            ImVec2(button_width, compact_button_height)))
        {
            g_currentPage = 2;
        }

        ImGui::PopStyleColor(1);
        ImGui::PopStyleVar(2);
        ImGui::PopFont();
    }
    ImGui::EndChild();
}

// Рисует рабочую область окна, правую часть
void DrawContentPaneBody(ImVec2 avail, float lPw)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32  child_bg = ImGui::GetColorU32(g_fillColorSub);
    const ImU32  child_border = ImGui::GetColorU32(ImGuiCol_Border);
    const ImVec2 child_size(avail.x - lPw, avail.y);

    const ImVec2 child_pos = ImVec2(lPw, g_titleColliderHeight);
    const float  cr = ImGui::GetStyle().ChildRounding;

    dl->AddRectFilled(
        child_pos,
        ImVec2(child_pos.x + child_size.x, child_pos.y + child_size.y),
        child_bg,
        cr,
        ImDrawFlags_RoundCornersTop
    );
    dl->AddRect(
        child_pos,
        ImVec2(child_pos.x + child_size.x, child_pos.y + child_size.y),
        child_border,
        cr,
        1.0f,
        ImDrawFlags_RoundCornersTop
    );

    ImGuiExt::ShadowBoxInner(
        child_pos,
        ImVec2(child_pos.x + child_size.x, child_pos.y + child_size.y),
        IM_COL32(0, 0, 0, 30),
        16.0f,
        cr
    );

    ImGui::SetCursorPos(child_pos);
    ImGui::BeginChild("Content", child_size, false);
    {
        switch (g_currentPage)
        {
        case 0: { // Library
            DrawLibraryPage();
            break;
        }
        case 1: { // Profile
            DrawProfilePage();
            break;
        }
        case 2: { // Settings
            DrawSettingsPage();
            break;
        }
        case 3: { // Accounts
            DrawAccountsPage();
            break;
        }
        default: break;
        }
    }
    ImGui::EndChild();
}

// Рисует карточку игры или же внутреннего Product
void DrawProductCard(Product* product, const ImVec2& card_size)
{
    const float padding = 2.0f;
    const float rounding = g_productCardRounding;
    const float aspect_ratio = 900.0f / 600.0f;

    const float icon_size = 28.0f;
    const float vertical_offs = 6.0f;
    const float text_gap = 5.0f;
    const float font_scale = 1.15f;

    ImVec2 base_card_pos = ImGui::GetCursorScreenPos();

    std::string btn_id = "##card_" + product->GetHash();
    ImGui::InvisibleButton(btn_id.c_str(), card_size);

    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();
    bool ready = product->IsReady();
    bool focused = hovered && ready;

    if (focused) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    ImVec2 final_pos = base_card_pos;
    ImVec2 final_size = card_size;

    ImU32 background = focused ? IM_COL32(60, 60, 60, 255) : IM_COL32(36, 36, 36, 255);
    ImU32 border = focused ? g_accentColor : ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_Border]);

    if (!ready)
    {
        background = IM_COL32(34, 34, 34, 200);
        border = IM_COL32(40, 40, 40, 200);
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImGuiExt::ShadowBoxOuter(
        final_pos, final_pos + final_size,
        IM_COL32(0, 0, 0, focused ? 35 : 30),
        focused ? 20.0f : 12.5f,
        g_productCardRounding
    );

    dl->AddRectFilled(final_pos, final_pos + final_size, background, rounding);
    dl->AddRect(final_pos, final_pos + final_size, border, rounding, 0, focused ? 1.8f : 1.0f);

    ImVec2 poster_pos = final_pos + ImVec2(padding, padding);
    ImVec2 poster_size(
        final_size.x - padding * 2.0f,
        (final_size.x - padding * 2.0f) * aspect_ratio
    );

    ID3D11ShaderResourceView* poster_tex = product->GetCurrentPoster();
    ImU32 poster_col = ready ? IM_COL32_WHITE : IM_COL32(180, 180, 180, 200);

    if (poster_tex)
    {
        dl->AddImageRounded(
            (ImTextureID)poster_tex,
            poster_pos,
            poster_pos + poster_size,
            ImVec2(0, 0), ImVec2(1, 1),
            poster_col,
            8.0f,
            ImDrawFlags_RoundCornersTop
        );
    }
    else
    {
        dl->AddRectFilled(
            poster_pos,
            poster_pos + poster_size,
            IM_COL32(45, 45, 45, 255),
            8.0f,
            ImDrawFlags_RoundCornersTop
        );
    }

    if (!product->IsMetaLoaded())
    {
        dl->AddRectFilled(
            poster_pos, poster_pos + poster_size,
            IM_COL32(0, 0, 0, 120), 8.0f,
            ImDrawFlags_RoundCornersTop
        );

        float time = (float)ImGui::GetTime();
        float angle = time * 4.0f;
        ImVec2 center(
            poster_pos.x + poster_size.x * 0.5f,
            poster_pos.y + poster_size.y * 0.5f
        );
        float radius = 12.0f;

        for (int i = 0; i < 8; i++)
        {
            float a = angle + i * 0.785f;
            float alpha = (float)i / 8.0f;
            ImVec2 dot_pos(
                center.x + cosf(a) * radius,
                center.y + sinf(a) * radius
            );
            dl->AddCircleFilled(dot_pos, 2.5f,
                IM_COL32(220, 220, 230, (int)(80 + 150 * alpha)));
        }
    }
    else
    {
        ServerStatus status = product->GetStatus();
        const char* badge_text = nullptr;
        ImU32 badge_bg = 0;

        if (status == SS_ComingSoon)
        {
            badge_text = "COMING SOON";
            badge_bg = IM_COL32(220, 120, 40, 230);
        }
        else if (status == SS_InMaintenance)
        {
            badge_text = "MAINTENANCE";
            badge_bg = IM_COL32(200, 180, 40, 230);
        }
        else if (status == SS_Offline)
        {
            badge_text = "OFFLINE";
            badge_bg = IM_COL32(180, 60, 60, 230);
        }
        else if (status == SS_Ready && !product->IsAvailable())
        {
            badge_text = "LOCKED";
            badge_bg = IM_COL32(80, 80, 90, 230);
        }

        if (badge_text)
        {
            ImFont* font = ImGui::GetFont();
            float badge_font_size = ImGui::GetFontSize() * 0.85f;
            ImVec2 badge_text_size = font->CalcTextSizeA(badge_font_size, FLT_MAX, 0.0f, badge_text);

            float badge_pad_x = 10.0f;
            float badge_pad_y = 4.0f;
            ImVec2 badge_size(
                badge_text_size.x + badge_pad_x * 2.0f,
                badge_text_size.y + badge_pad_y * 2.0f
            );

            ImVec2 badge_pos(
                poster_pos.x + poster_size.x - badge_size.x - 8.0f,
                poster_pos.y + 8.0f
            );

            dl->AddRectFilled(badge_pos, badge_pos + badge_size, badge_bg, 6.0f);

            dl->AddText(font, badge_font_size,
                ImVec2(badge_pos.x + badge_pad_x, badge_pos.y + badge_pad_y),
                IM_COL32(255, 255, 255, 255), badge_text);
        }
    }

    const std::string& title = product->GetTitle();

    ImFont* font = ImGui::GetFont();
    const float font_size = ImGui::GetFontSize() * font_scale;
    ImVec2 text_size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, title.c_str());

    const float block_width = icon_size + text_gap + text_size.x;
    const float block_offset_x = (final_size.x - block_width) * 0.5f;
    const float block_y = poster_pos.y + poster_size.y + vertical_offs;

    ImVec2 icon_pos(final_pos.x + block_offset_x, block_y);

    ID3D11ShaderResourceView* icon_tex = product->GetCurrentIcon();
    ImU32 icon_col = ready ? IM_COL32_WHITE : IM_COL32(180, 180, 180, 200);

    if (icon_tex)
    {
        dl->AddImageRounded(
            (ImTextureID)icon_tex,
            icon_pos,
            icon_pos + ImVec2(icon_size, icon_size),
            ImVec2(0, 0), ImVec2(1, 1),
            icon_col,
            5.0f
        );
    }
    else
    {
        dl->AddRectFilled(
            icon_pos,
            icon_pos + ImVec2(icon_size, icon_size),
            IM_COL32(55, 55, 55, 255),
            5.0f
        );

        if (!title.empty())
        {
            char letter[2] = { (char)std::toupper((unsigned char)title[0]), 0 };
            float letter_font_size = icon_size * 0.55f;
            ImVec2 letter_size = font->CalcTextSizeA(letter_font_size, FLT_MAX, 0.0f, letter);
            ImVec2 letter_pos(
                icon_pos.x + (icon_size - letter_size.x) * 0.5f,
                icon_pos.y + (icon_size - letter_size.y) * 0.5f
            );
            dl->AddText(font, letter_font_size, letter_pos,
                IM_COL32(180, 180, 190, 255), letter);
        }
    }

    ImVec2 text_pos(
        icon_pos.x + icon_size + text_gap,
        block_y + (icon_size - text_size.y) * 0.5f
    );
    ImU32 text_color = ready ? IM_COL32(230, 230, 230, 255) : IM_COL32(150, 150, 150, 255);

    dl->AddText(font, font_size, text_pos, text_color, title.c_str());

    if (clicked && ready)
    {
        g_productPopupOpen = true;
        p_currentProduct = product;
    }
}

// Рисует карточку аккаунта стима
bool DrawAccountCard(const SteamAccount& acc, bool is_current, float width)
{
    ImDrawList*  dl = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const float  height = 56.0f;
    const float  rounding = 10.0f;
    const float  icon_size = 40.0f;
    const ImVec2 size(width, height);

    std::string  id = "##acc_" + std::to_string(acc.GetSteamID64());
    ImGui::InvisibleButton(id.c_str(), size);

    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();

    if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
    {
        uint64_t id64 = acc.GetSteamID64();
        ImGui::SetDragDropPayload("STEAM_ACCOUNT", &id64, sizeof(uint64_t));

        ImGui::Text("%s", acc.GetPersonaName().c_str());
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "@%s", acc.GetAccountName().c_str());

        ImGui::EndDragDropSource();
    }

    ImU32 col_top = IM_COL32(6, 20, 48, 255);
    ImU32 col_bottom = IM_COL32(23, 60, 97, 255);

    if (hovered)
    {
        col_top = IM_COL32(9, 23, 51, 255);
        col_bottom = IM_COL32(26, 63, 100, 255);
    }

    if (is_current)
    {
        col_top = IM_COL32(10, 30, 68, 255);
        col_bottom = IM_COL32(33, 82, 128, 255);
    }
    
    ImGuiExt::ShadowBoxOuter(
        pos, ImVec2(pos.x + size.x, pos.y + size.y),
        IM_COL32(0, 0, 0, 30),
        12.5f,
        rounding
    );

    ImGuiExt::AddRectFilledMultiColor(
        pos, ImVec2(pos.x + size.x, pos.y + size.y),
        col_top, col_bottom, rounding, ImDrawFlags_RoundCornersAll
    );

    ImU32 border_col;
    float border_thickness;

    if (!acc.CanAutoLogin())
    {
        border_col = IM_COL32(230, 40, 7, 200);
        border_thickness = 1.5f;
    }
    else
    {
        border_col = IM_COL32(36, 36, 35, 255);
        border_thickness = 1.25f;
    }

    dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
        border_col, rounding, 0, border_thickness);

    const float icon_pad = (height - icon_size) * 0.5f;
    ImVec2 icon_pos(pos.x + icon_pad, pos.y + icon_pad);

    dl->AddRectFilled(icon_pos, ImVec2(icon_pos.x + icon_size, icon_pos.y + icon_size),
        IM_COL32(20, 20, 30, 255), 8.0f);

    if (acc.GetAvatar() && acc.GetAvatarState() == SteamAccount::AvatarState::Loaded)
    {
        dl->AddImageRounded(
            (ImTextureID)acc.GetAvatar(),
            icon_pos, ImVec2(icon_pos.x + icon_size, icon_pos.y + icon_size),
            ImVec2(0, 0), ImVec2(1, 1),
            IM_COL32_WHITE, 8.0f
        );
    }
    else if (acc.GetAvatarState() == SteamAccount::AvatarState::Loading)
    {
        float time = (float)ImGui::GetTime();
        float angle = time * 4.0f;
        ImVec2 center(icon_pos.x + icon_size * 0.5f, icon_pos.y + icon_size * 0.5f);
        float radius = icon_size * 0.25f;

        for (int i = 0; i < 8; i++)
        {
            float a = angle + i * 0.785f;
            float alpha = (float)i / 8.0f;
            ImVec2 dot_pos(center.x + cosf(a) * radius, center.y + sinf(a) * radius);
            dl->AddCircleFilled(dot_pos, 2.0f,
                IM_COL32(200, 200, 220, (int)(80 + 150 * alpha)));
        }
    }
    else
    {
        const std::string& name = acc.GetPersonaName();
        if (!name.empty())
        {
            char letter[2] = { (char)std::toupper(name[0]), 0 };
            ImFont* font = ImGui::GetFont();
            float letter_size = icon_size * 0.5f;
            ImVec2 text_size = font->CalcTextSizeA(letter_size, FLT_MAX, 0.0f, letter);
            ImVec2 text_pos(
                icon_pos.x + (icon_size - text_size.x) * 0.5f,
                icon_pos.y + (icon_size - text_size.y) * 0.5f
            );
            dl->AddText(font, letter_size, text_pos, IM_COL32(200, 200, 220, 255), letter);
        }
    }

    dl->AddRect(icon_pos, ImVec2(icon_pos.x + icon_size, icon_pos.y + icon_size),
        IM_COL32(255, 255, 255, 40), 8.0f, 0, 1.0f);

    ImFont* font = ImGui::GetFont();
    const float name_font_size = ImGui::GetFontSize() * 1.15f;
    const float login_font_size = ImGui::GetFontSize() * 0.95f;

    float text_x = icon_pos.x + icon_size + 12.0f;
    float text_y = pos.y + icon_pad;

    dl->AddText(font, name_font_size, ImVec2(text_x, text_y),
        IM_COL32(255, 255, 255, 255), acc.GetPersonaName().c_str());

    std::string login = "@" + acc.GetAccountName();
    dl->AddText(font, login_font_size,
        ImVec2(text_x, text_y + name_font_size + 2.0f),
        IM_COL32(180, 195, 210, 200), login.c_str());

    const char* status = (acc.CanAutoLogin() ? "Logged in" : "Log-in required");

    ImU32 status_col = is_current ? IM_COL32(80, 200, 255, 255)
        : (acc.CanAutoLogin() ? IM_COL32(30, 240, 60, 255) : IM_COL32(230, 40, 7, 255));

    ImVec2 status_size = font->CalcTextSizeA(login_font_size, FLT_MAX, 0.0f, status);

    float badge_padding_x = 10.0f;
    float badge_padding_y = 4.0f;
    ImVec2 badge_min(
        pos.x + size.x - status_size.x - badge_padding_x * 2.0f - 12.0f,
        pos.y + (size.y - status_size.y - badge_padding_y * 2.0f) * 0.5f
    );
    ImVec2 badge_max(
        badge_min.x + status_size.x + badge_padding_x * 2.0f,
        badge_min.y + status_size.y + badge_padding_y * 2.0f
    );

    ImU32 badge_bg = IM_COL32(
        (int)((status_col >> IM_COL32_R_SHIFT) & 0xFF),
        (int)((status_col >> IM_COL32_G_SHIFT) & 0xFF),
        (int)((status_col >> IM_COL32_B_SHIFT) & 0xFF),
        40
    );

    if (!is_current) {
        dl->AddRectFilled(badge_min, badge_max, badge_bg, 12.0f);
        dl->AddRect(badge_min, badge_max, status_col, 12.0f, 0, 1.0f);

        dl->AddText(font, login_font_size,
            ImVec2(badge_min.x + badge_padding_x, badge_min.y + badge_padding_y),
            status_col, status);
    }

    return clicked;
}

// Рисует страницу библиотеки игр
void DrawLibraryPage()
{
    std::vector<Product*> products = ProductRegistry::GetAllProducts();
    const int product_count = products.size();

    const float aspect_ratio = 900.0f / 600.0f;
    const float padding = 20.0f;
    const float card_gap = 20.0f;

    const float icon_and_text_zone = 40.0f;

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float card_height = avail.y - padding * 2.0f;

    const float poster_height = card_height - icon_and_text_zone;
    const float card_width = poster_height / aspect_ratio;

    const ImVec2 card_size(card_width, card_height);

    ImGuiWindowFlags flags = ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollbar;

    ImGui::BeginChild("LibraryScrollArea", ImVec2(0, 0), false, flags);
    {
        if (ImGui::IsWindowHovered())
        {
            float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f)
            {
                float scroll_speed = card_width * g_libraryScrollSpeedMultiplier;
                ImGui::SetScrollX(ImGui::GetScrollX() - wheel * scroll_speed);
            }
        }

        ImGui::Dummy(ImVec2(padding - ImGui::GetStyle().ItemSpacing.x, 0.0f));
        ImGui::SameLine();

        float cursor_y_start = ImGui::GetCursorPosY() + padding;

        for (int i = 0; i < product_count; i++)
        {
            if (i > 0)
            {
                ImGui::SameLine(0.0f, card_gap);
            }

            ImGui::SetCursorPosY(cursor_y_start);

            DrawProductCard(products[i], card_size);
        }

        ImGui::SameLine(0.0f, padding - ImGui::GetStyle().ItemSpacing.x);
        ImGui::Dummy(ImVec2(1.0f, 1.0f));

        ImVec2 win_pos = ImGui::GetWindowPos();
        ImVec2 win_size = ImGui::GetWindowSize();
        const float scroll_x = ImGui::GetScrollX();
        const float scroll_max = ImGui::GetScrollMaxX();
        const float scroll_x_from_right = scroll_max - scroll_x;

        const float shadow_width = 60.0f;
        const float fade_distance = 80.0f;
        const float max_shadow_alpha = 100.0f;
        const float shadow_rounding = ImGui::GetStyle().ChildRounding;

        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        if (scroll_x > 1.0f)
        {
            float t = ImClamp(scroll_x / fade_distance, 0.0f, 1.0f);
            ImU32 shadow_col = IM_COL32(0, 0, 0, (int)(max_shadow_alpha * t));
            ImU32 transparent = IM_COL32(0, 0, 0, 0);

            ImVec2 p_min = win_pos;
            ImVec2 p_max = ImVec2(win_pos.x + shadow_width, win_pos.y + win_size.y);

            ImGuiExt::AddRectFilledMultiColorHorizontal(
                p_min, p_max,
                shadow_col,
                transparent,
                shadow_rounding,
                ImDrawFlags_RoundCornersTopLeft
            );
        }

        if (scroll_x_from_right > 1.0f)
        {
            float t = ImClamp(scroll_x_from_right / fade_distance, 0.0f, 1.0f);
            ImU32 shadow_col = IM_COL32(0, 0, 0, (int)(max_shadow_alpha * t));
            ImU32 transparent = IM_COL32(0, 0, 0, 0);

            ImVec2 p_min = ImVec2(win_pos.x + win_size.x - shadow_width, win_pos.y);
            ImVec2 p_max = ImVec2(win_pos.x + win_size.x, win_pos.y + win_size.y);

            ImGuiExt::AddRectFilledMultiColorHorizontal(
                p_min, p_max,
                transparent,
                shadow_col,
                shadow_rounding,
                ImDrawFlags_RoundCornersTopRight
            );
        }
    }
    ImGui::EndChild();
}

// Рисует страницу настроек
void DrawSettingsPage()
{
    const float page_padding = 24.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(page_padding, page_padding));
    ImGui::BeginChild("SettingsScroll", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_None);
    {
        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.6f);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
        ImGui::TextUnformatted("Settings");
        ImGui::PopStyleColor();
        ImGui::PopFont();

        ImGui::Dummy(ImVec2(0, 16.0f));

        SettingsSectionHeader("Colors");

        SettingsColorEditU32("Accent Color", &g_accentColor,
            "Main accent color used for buttons, indicators and highlights");

        SettingsColorEditU32("Accent Color (Hover)", &g_accentColorSub,
            "Accent color when hovering over interactive elements");

        SettingsColorEditVec4("Main Fill Color", &g_fillColorMain,
            "Primary background color of the main window");

        SettingsColorEditVec4("Sub Fill Color", &g_fillColorSub,
            "Background color of secondary panels (like the content area)");

        SettingsSectionHeader("Layout");

        SettingsSliderInt("Title Region Height", &g_titleRegionHeight, 30, 100,
            "Total height of the title bar area at the top of the window");

        SettingsSliderInt("Title Collider Height", &g_titleColliderHeight, 20, 60,
            "Draggable area height for moving the window");

        SettingsSliderInt("Frame Offset X", &g_frameOffsetX, 0, 200,
            "Horizontal offset of the main content frame");

        SettingsSliderInt("Frame Offset Y", &g_frameOffsetY, 0, 200,
            "Vertical offset of the main content frame");

        SettingsSliderInt("Control Panel Width", &g_controlPanelWidth, 80, 250,
            "Width of the left sidebar with navigation buttons");

        SettingsSectionHeader("Product Cards");

        SettingsSliderFloat("Card Width", &g_productCardWidth, 100.0f, 300.0f, "%.0f px",
            "Base width of product cards in the library");

        SettingsSliderFloat("Card Rounding", &g_productCardRounding, 0.0f, 30.0f, "%.1f",
            "Corner radius of product cards");

        SettingsSectionHeader("Actions");

        ImGui::Dummy(ImVec2(4.0f, 0));
        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(60, 60, 60, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(80, 80, 80, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(100, 100, 100, 255));

        if (ImGui::Button("Reset to Defaults", ImVec2(160.0f, 32.0f)))
        {
            g_accentColor = ImColor(230, 25, 52, 255);
            g_accentColorSub = ImColor(250, 82, 101, 255);
            g_fillColorMain = ImColor(23, 23, 23, 255);
            g_fillColorSub = ImColor(31, 30, 30, 255);
            g_titleRegionHeight = 54;
            g_titleColliderHeight = 35;
            g_frameOffsetX = 65;
            g_frameOffsetY = 48;
            g_controlPanelWidth = 115;
            g_productCardWidth = 155.0f;
            g_productCardRounding = 10.0f;
        }

        ImGui::PopStyleColor(3);

        ImGui::SameLine(0.0f, 12.0f);

        ImGui::PushStyleColor(ImGuiCol_Button, g_accentColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, g_accentColorSub);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, g_accentColor);

        ImGui::PopStyleColor(3);

        ImGui::Dummy(ImVec2(0, page_padding));
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

// Рисует страницу аккаунтов Steam
void DrawAccountsPage()
{
    if (!g_accountsLoaded)
        RefreshSteamAccounts();

    const float page_padding = 24.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(page_padding, page_padding));
    ImGui::BeginChild("AccountsScroll", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    {
        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.6f);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
        ImGui::TextUnformatted("Steam Accounts");
        ImGui::PopStyleColor();
        ImGui::PopFont();

        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(150, 150, 150, 255));
        ImGui::TextUnformatted("Drag accounts between sections to change your current login");
        ImGui::PopStyleColor();

        ImGui::SameLine();
        float refresh_x = ImGui::GetContentRegionAvail().x - 100.0f;
        float btn_height = ImGui::GetTextLineHeight() * 2.3f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + refresh_x);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ImGui::GetStyle().ChildRounding);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(35.0f / 255.0f, 35.0f / 255.0f, 35.0f / 255.0f, 1.00f));
        if (ImGuiExt::Button("Refresh", g_accentColor, g_accentColorSub, ImVec2(100.0f, btn_height)))
            RefreshSteamAccounts();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        ImGui::Dummy(ImVec2(0, 12.0f));

        const float content_width = ImGui::GetContentRegionAvail().x;
        const float card_height = 56.0f;
        const float container_inner_padding = 12.0f;   // Отступ между картами и рамкой контейнера
        const ImU32 element_fill_col = IM_COL32(18, 18, 18, 255);
        const ImU32 element_border_col = ImGui::GetColorU32(ImGuiCol_Border);
        const float container_rounding = 10.0f;

        const float current_container_height = card_height + container_inner_padding * 2.0f;

        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.05f);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200, 200, 200, 255));
        ImGui::TextUnformatted("Current:");
        ImGui::PopStyleColor();
        ImGui::PopFont();

        ImGui::Dummy(ImVec2(0, 4.0f));

        ImVec2 current_container_pos = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        ImGuiExt::ShadowBoxOuter(
            current_container_pos,
            ImVec2(current_container_pos.x + content_width,
                current_container_pos.y + current_container_height),
            IM_COL32(0, 0, 0, 30),
            15.0f,
            container_rounding
        );
        dl->AddRectFilled(
            current_container_pos,
            ImVec2(current_container_pos.x + content_width,
                current_container_pos.y + current_container_height),
            element_fill_col, container_rounding);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
            ImVec2(container_inner_padding, container_inner_padding));
        ImGui::BeginChild("CurrentContainer",
            ImVec2(content_width, current_container_height),
            ImGuiChildFlags_AlwaysUseWindowPadding);
        {
            const float inner_width = ImGui::GetContentRegionAvail().x;

            if (g_currentAccount)
            {
                if (DrawAccountCard(*g_currentAccount, true, inner_width))
                {
                    g_currentAccount = nullptr;
                    SteamHelper::SetAutoLoginUser("");
                }
            }
            else
            {
                const char* msg = "Drop an account here or click one below";
                ImVec2 text_size = ImGui::CalcTextSize(msg);
                ImVec2 avail = ImGui::GetContentRegionAvail();

                ImGui::SetCursorPos(ImVec2(
                    (ImGui::GetWindowWidth() - text_size.x) * 0.5f,
                    (ImGui::GetWindowHeight() - text_size.y) * 0.5f
                ));
                ImGui::TextColored(ImVec4(0.47f, 0.47f, 0.49f, 1.0f), "%s", msg);
            }
        }
        ImGui::EndChild();

        dl->AddRect(
            current_container_pos,
            ImVec2(current_container_pos.x + content_width,
                current_container_pos.y + current_container_height),
            element_border_col, container_rounding, 0, 1.0f);

        ImGui::PopStyleVar();

        ImGui::SetCursorScreenPos(current_container_pos);
        ImGui::InvisibleButton("##current_drop_target",
            ImVec2(content_width, current_container_height));

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("STEAM_ACCOUNT"))
            {
                uint64_t dropped_id = *(const uint64_t*)payload->Data;
                for (auto& acc : g_steamAccounts)
                {
                    if (acc.GetSteamID64() == dropped_id)
                    {
                        g_currentAccount = &acc;
                        SteamHelper::SetAutoLoginUser(acc.GetAccountName());
                        break;
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::Dummy(ImVec2(0, 16.0f));

        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.05f);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200, 200, 200, 255));
        ImGui::TextUnformatted("Available:");
        ImGui::PopStyleColor();
        ImGui::PopFont();

        ImGui::Dummy(ImVec2(0, 4.0f));

        ImVec2 available_list_pos = ImGui::GetCursorScreenPos();
        ImVec2 window_content_max = ImGui::GetWindowPos() + ImGui::GetWindowSize();
        float available_height = window_content_max.y - available_list_pos.y - page_padding;
        available_height = ImMax(available_height, card_height + container_inner_padding * 2.0f);

        ImGuiExt::ShadowBoxOuter(
            available_list_pos,
            ImVec2(available_list_pos.x + content_width, available_list_pos.y + available_height),
            IM_COL32(0, 0, 0, 30),
            15.0f,container_rounding);
        dl->AddRectFilled(
            available_list_pos,
            ImVec2(available_list_pos.x + content_width, available_list_pos.y + available_height),
            element_fill_col, container_rounding);
        dl->AddRect(
            available_list_pos,
            ImVec2(available_list_pos.x + content_width, available_list_pos.y + available_height),
            element_border_col, container_rounding, 0, 1.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
            ImVec2(container_inner_padding, container_inner_padding));
        ImGui::BeginChild("AvailableList",
            ImVec2(content_width, available_height),
            ImGuiChildFlags_AlwaysUseWindowPadding);
        {
            const float inner_width = ImGui::GetContentRegionAvail().x;
            const float card_gap = 8.0f;

            bool any_shown = false;
            for (auto& acc : g_steamAccounts)
            {
                if (g_currentAccount && acc.GetSteamID64() == g_currentAccount->GetSteamID64())
                    continue;

                if (DrawAccountCard(acc, false, inner_width))
                {
                    g_currentAccount = &acc;
                    SteamHelper::SetAutoLoginUser(acc.GetAccountName());
                }

                ImGui::Dummy(ImVec2(0, card_gap));
                any_shown = true;
            }

            if (!any_shown)
            {
                const char* msg = "No other accounts available";
                ImVec2 text_size = ImGui::CalcTextSize(msg);

                ImGui::SetCursorPos(ImVec2(
                    (ImGui::GetWindowWidth() - text_size.x) * 0.5f,
                    (ImGui::GetWindowHeight() - text_size.y) * 0.5f
                ));
                ImGui::TextColored(ImVec4(0.47f, 0.47f, 0.49f, 1.0f), "%s", msg);
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();

        ImGui::SetCursorScreenPos(available_list_pos);
        ImGui::InvisibleButton("##available_drop_target",
            ImVec2(content_width, available_height));

        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("STEAM_ACCOUNT"))
            {
                uint64_t dropped_id = *(const uint64_t*)payload->Data;
                if (g_currentAccount && g_currentAccount->GetSteamID64() == dropped_id)
                {
                    g_currentAccount = nullptr;
                    SteamHelper::SetAutoLoginUser("");
                }
            }
            ImGui::EndDragDropTarget();
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

// Рисует страницу профиля 
void DrawProfilePage()
{
    if (!g_systemInfoLoaded)
    {
        g_systemInfo = SystemInfoCollector::Collect(g_pd3dDevice);
        g_systemInfoLoaded = true;
    }

    const float page_padding = 24.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(page_padding, page_padding));
    ImGui::BeginChild("ProfileScroll", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
    {
        ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.6f);
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
        ImGui::TextUnformatted("Profile");
        ImGui::PopStyleColor();
        ImGui::PopFont();

        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(150, 150, 150, 255));
        ImGui::TextUnformatted("Your account and hardware information");
        ImGui::PopStyleColor();

        ImVec2 current_container_pos = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        RemoteProfile* profile = ProductRegistry::s_profile.get();

        DrawCard(IM_COL32(18, 18, 18, 255), 100.0f, [profile]() {
            if (profile)
            {
                ImGui::Text("Hello, ");
                ImGui::SameLine();
                ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.3f);
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
                ImGui::TextUnformatted(profile->GetUsername().c_str());
                ImGui::PopStyleColor();
                ImGui::PopFont();
            }
            else
            {
                ImGui::SameLine();
                ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.2f);
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(160, 160, 160, 255));
                ImGui::TextUnformatted("You are not logged in.");
                ImGui::PopStyleColor();
                ImGui::PopFont();
            }
            });

        ImGui::Dummy(ImVec2(0, 16.0f));

        DrawCardSimple("System", 205.0f, []() {
            LabelValueRowCopyable("Operating System", g_systemInfo.windows_version.c_str());
            LabelValueRowCopyable("Build", g_systemInfo.windows_build.c_str(),
                true, g_systemInfo.windows_build.c_str());
            LabelValueRowCopyable("Architecture", g_systemInfo.os_arch.c_str());

            LabelValueRowCopyable("CPU", g_systemInfo.cpu_name.c_str(),
                true, g_systemInfo.cpu_name.c_str());

            LabelValueRowCopyable("RAM", g_systemInfo.ram_total.c_str());

            LabelValueRowCopyable("GPU", g_systemInfo.gpu_name.c_str(),
                true, g_systemInfo.gpu_name.c_str());
            LabelValueRowCopyable("VRAM", g_systemInfo.gpu_vram.c_str());
            });

        DrawCardSimple("HWID", 92.5f, []() {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 pos = ImGui::GetCursorScreenPos();
            float width = ImGui::GetContentRegionAvail().x;

            ImGui::Dummy(ImVec2(0, 4.0f));

            ImFont* font = ImGui::GetFont();
            float hwid_font_size = ImGui::GetFontSize() * 1.12f;
            ImVec2 hwid_size = font->CalcTextSizeA(hwid_font_size, FLT_MAX, 0.0f,
                g_systemInfo.hwid.c_str());

            float hwid_x = pos.x + 24.0f;
            float hwid_y = ImGui::GetCursorScreenPos().y;

            dl->AddText(font, hwid_font_size,
                ImVec2(hwid_x, hwid_y),
                IM_COL32(220, 220, 230, 255), g_systemInfo.hwid.c_str());

            float btn_w = 125.0f;
            float btn_h = 24.0f;

            ImGui::Dummy(ImVec2(width - btn_w - 28.0f, 0));

            ImGui::SameLine();

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ImGui::GetStyle().ChildRounding);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(35.0f / 255.0f, 35.0f / 255.0f, 35.0f / 255.0f, 1.00f));
            if (ImGuiExt::Button("Copy HWID", g_accentColor, g_accentColorSub, ImVec2(btn_w, btn_h)))
            {
                ImGui::SetClipboardText(g_systemInfo.hwid_full.c_str());
            }
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            });

//        DrawCardSimple("Processor", 125.0f, []() {
//            LabelValueRowCopyable("Cores", g_systemInfo.cpu_cores.c_str());
//            });

//        DrawCardSimple("Memory", 125.0f, []() {
//            LabelValueRowCopyable("Total RAM", g_systemInfo.ram_total.c_str());
//            LabelValueRowCopyable("Status", g_systemInfo.ram_available.c_str());
//            });

//        DrawCardSimple("Graphics", 125.0f, []() {
//            LabelValueRowCopyable("Adapter", g_systemInfo.gpu_name.c_str(),
//                true, g_systemInfo.gpu_name.c_str());
//            LabelValueRowCopyable("VRAM", g_systemInfo.gpu_vram.c_str());
//            });
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
}

// Создает необходимые ресурсы для работы гуи
void CreateResources()
{
    g_systemInfo = SystemInfoCollector::Collect(g_pd3dDevice);
    g_systemInfoLoaded = true;

    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // Загрузка пользовательского шрифта
    auto rootDir = GetExecutableDirectory();
    // Путь {ПАПКА_С_Loader.exe}/Resources/Fonts/Roboto-Regular.ttf
    auto fontPath = rootDir / L"Resources" / L"Fonts" / L"Roboto-Regular.ttf";
    // Используем Roboto-Regular.ttf и увеличиваем размер шрифта на +3.0 от базового
    {
        const float base_sz = (io.Fonts->Fonts.Size > 0) ? io.Fonts->Fonts[0]->LegacySize : 13.0f;
        const float title_sz = base_sz + 3.0f;
        g_titleFont = ImGui::GetIO().Fonts->AddFontFromFileTTF(
            fontPath.string().c_str(),
            title_sz,
            nullptr,
            ImGui::GetIO().Fonts->GetGlyphRangesCyrillic()
        );
        // Обновляем графические объекты бэкенда для нового атласа шрифтов
        ImGui_ImplDX11_CreateDeviceObjects();
    }

    auto titleIconPath = rootDir / L"Resources" / L"Icons" / L"nemesis.png";
    LoadTextureFromFile(titleIconPath.c_str(), g_pd3dDevice, &g_titleIcon);

    auto libraryIconPath = rootDir / L"Resources" / L"Icons" / L"console.png";
    LoadTextureFromFile(libraryIconPath.c_str(), g_pd3dDevice, &g_libraryIcon);

    auto profileIconPath = rootDir / L"Resources" / L"Icons" / L"user.png";
    LoadTextureFromFile(profileIconPath.c_str(), g_pd3dDevice, &g_profileIcon);

    auto settingsIconPath = rootDir / L"Resources" / L"Icons" / L"gear.png";
    LoadTextureFromFile(settingsIconPath.c_str(), g_pd3dDevice, &g_settingsIcon);

    auto accountsIconPath = rootDir / L"Resources" / L"Icons" / L"steam.png";
    LoadTextureFromFile(accountsIconPath.c_str(), g_pd3dDevice, &g_accountsIcon);

    NemesisAPI::Initialize("https://raw.githubusercontent.com/xw1w1/Nemesis/main");

    ProductRegistry::Initialize(g_pd3dDevice);
    ProductRegistry::Sync();

    AvatarLoader::Initialize(g_pd3dDevice, [](uint64_t id) -> SteamAccount* {
        for (auto& acc : g_steamAccounts)
            if (acc.GetSteamID64() == id) return &acc;
        return nullptr;
        });
    RefreshSteamAccounts();
}

void RefreshSteamAccounts()
{
    g_steamAccounts = SteamHelper::GetAllAccounts();
    g_currentAccount = nullptr;

    std::string autoLogin = SteamHelper::GetAutoLoginUser();
    for (auto& acc : g_steamAccounts)
    {
        if (!autoLogin.empty() && acc.GetAccountName() == autoLogin)
        {
            g_currentAccount = &acc;
            break;
        }
    }
    if (!g_currentAccount)
    {
        for (auto& acc : g_steamAccounts)
        {
            if (acc.IsMostRecent()) { g_currentAccount = &acc; break; }
        }
    }
    g_accountsLoaded = true;

    for (auto& acc : g_steamAccounts)
    {
        AvatarLoader::RequestAvatar(acc.GetSteamID64());
    }
}

// Хелперы DirectX 11

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
        const int PAD = 8;          // отступ справа/сверху у кнопок
        const int BTN = 22;         // размер кнопок
        const int SP = 6;           // зазор между кнопками

        // Прямоугольники кнопок и потенциальных чекбоксов (которые появляются при наведении)
        RECT rClose{ W - PAD - BTN, PAD, W - PAD, PAD + BTN };
        RECT rMin{ W - PAD - 2 * BTN - SP, PAD, W - PAD - BTN - SP, PAD + BTN };
        RECT rCBClose{ rClose.left + 2, rClose.bottom + 4, rClose.left + 2 + BTN - 4, rClose.bottom + 4 + BTN - 4 };
        RECT rCBMin{ rMin.left + 2, rMin.bottom + 4, rMin.left + 2 + BTN - 4, rMin.bottom + 4 + BTN - 4 };

        auto in_rect = [](POINT p, const RECT& r) -> bool {
            return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom;
            };

        if (in_rect(pt, rClose) || in_rect(pt, rMin) || in_rect(pt, rCBClose) || in_rect(pt, rCBMin))
            return HTCLIENT; // над элементами управления — не двигаем

        // Пустые зоны окна — можно тянуть
        if (pt.y < TOP_BAR)
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
