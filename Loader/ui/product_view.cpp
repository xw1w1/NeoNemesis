#pragma region Includes
#include "product_view.h"

#include "blur.h"
#include "imgui_extensions.h"
#include "imgui.h"
#include "imgui_internal.h"

#include "../process_helper.h"
#include "../steam/steam_helper.h"
#include "../injector/infector.h"

#include "Process/Process.h"
#include "ManualMap/MMap.h"

#include <cmath>
#include <cstdio>
#include <shellapi.h>

#pragma endregion

// Нужен доступ к g_currentAccount из main
// Можно передавать через параметр или extern
extern SteamAccount* g_currentAccount;
extern HWND g_Hwnd;
extern bool g_isAppDone;

void ProductView::Open(Product* product)
{
    if (!product) return;

    product_ = product;
    open_ = true;
    bgDirty_ = true;
    runState_ = RS_Idle;
    runTimer_ = 0.0f;
    injectStatus_ = HINFRES::EMPTY;
    appDone_ = false;
}

void ProductView::Close()
{
    open_ = false;
    runState_ = RS_Idle;
    runTimer_ = 0.0f;
}

bool ProductView::IsActive() const
{
    return anim_ > 0.001f;
}

bool ProductView::IsFullyOpen() const
{
    return anim_ > 0.999f;
}

bool ProductView::BlocksInput() const
{
    return anim_ > 0.01f;
}

void ProductView::Update(float deltaTime)
{
    float target = open_ ? 1.0f : 0.0f;
    float blend = 1.0f - std::pow(0.00008f, deltaTime);
    anim_ += (target - anim_) * blend;
    if (std::fabs(anim_ - target) < 0.002f)
        anim_ = target;

    if (runState_ != RS_Idle && runState_ != RS_Finished) UpdateRunState(deltaTime);
}

void ProductView::UpdateRunState(float dt)
{
    if (!product_) return;

    runTimer_ -= dt;
    if (runTimer_ > 0.0f) return;

    switch (runState_)
    {
    case RS_ClosingSteam:
    {
        bool steamRunning = IsProcessRunning(L"steam.exe");
        bool helperRunning = IsProcessRunning(L"steamwebhelper.exe");

        if (steamRunning || helperRunning)
        {
            static bool tried_soft = false;
            if (!tried_soft)
            {
                SteamHelper::ShutdownSteam();
                tried_soft = true;
                runTimer_ = 3.0f;
            }
            else
            {
                KillProcessByName(L"steam.exe");
                KillProcessByName(L"steamwebhelper.exe");
                runTimer_ = 1.5f;
                tried_soft = false;
            }
        }
        else
        {
            runState_ = RS_CheckingFiles;
            runTimer_ = 0.5f;
        }
        break;
    }
    case RS_CheckingFiles:
    {
        if (SteamHelper::IsSteamReady())
            runState_ = RS_LaunchingSteamGame;
        else
            runState_ = RS_LaunchingSteam;
        runTimer_ = 0.5f;
        break;
    }
    case RS_LaunchingSteam:
    {
        if (g_currentAccount)
        {
            SteamHelper::LaunchSteamAs(*g_currentAccount);
        }
        else
        {
            std::string steamPath = SteamHelper::GetSteamInstallPath();
            if (!steamPath.empty())
                ShellExecuteA(NULL, "open",
                    (steamPath + "\\steam.exe").c_str(),
                    NULL, NULL, SW_SHOWNORMAL);
        }
        runState_ = RS_WaitingForSteam;
        runTimer_ = 3.0f;
        break;
    }
    case RS_WaitingForSteam:
    {
        static float totalWait = 0.0f;
        totalWait += 1.0f;
        if (SteamHelper::IsSteamReady())
        {
            runState_ = RS_LaunchingSteamGame;
            runTimer_ = 1.5f;
            totalWait = 0.0f;
        }
        else if (totalWait > 60.0f)
        {
            runState_ = RS_Idle;
            totalWait = 0.0f;
        }
        else
        {
            runTimer_ = 1.0f;
        }
        break;
    }
    case RS_LaunchingSteamGame:
    {
        auto path = std::wstring(L"steam://rungameid/")
            + std::to_wstring(product_->GetSteamId());
        ShellExecuteW(NULL, L"open", path.c_str(), NULL, NULL, SW_SHOWNORMAL);
        runState_ = RS_WaitingForGame;
        runTimer_ = 2.0f;
        break;
    }
    case RS_WaitingForGame:
    {
        if (IsProcessWindowVisible(product_->GetProcNameW().c_str()))
        {
            runState_ = RS_Countdown;
            runTimer_ = (float)product_->GetDefaultInjectTime();
        }
        else
        {
            runTimer_ = 1.0f;
        }
        break;
    }
    case RS_Countdown:
    {
        runState_ = RS_Finalizing;
        runTimer_ = 1.0f;
        break;
    }
    case RS_Finalizing:
    {
        if (Injector::IsWindows10OrLower())
        {
            nemesis::Process proc;
            if (NT_SUCCESS(proc.Attach(product_->GetProcNameW().c_str())))
                proc.mmap().MapImage(L"NemesisLoader.dll", nemesis::NoThreads);
        }
        else
        {
            injectStatus_ = Injector::Inject(
                L"NemesisLoader.dll",
                product_->GetProcNameW().c_str());
        }
        runState_ = RS_CheckingCrash;
        runTimer_ = 8.0f;
        break;
    }
    case RS_CheckingCrash:
    {
        if (!IsProcessRunning(product_->GetProcNameW().c_str()))
            runState_ = RS_Idle;
        else
            runState_ = RS_Finished;
        break;
    }
    case RS_Finished:
    {
        appDone_ = true;
        break;
    }
    default: break;
    }
}

std::string ProductView::GetButtonText() const
{
    if (!product_) return "...";

    switch (runState_)
    {
    case RS_ClosingSteam:       return "Closing Steam...";
    case RS_CheckingFiles:      return "Verifying files...";
    case RS_LaunchingSteam:     return "Launching Steam...";
    case RS_WaitingForSteam:    return "Waiting for Steam...";
    case RS_LaunchingSteamGame: return "Starting game...";
    case RS_WaitingForGame:     return "Waiting for game...";
    case RS_Countdown:
    {
        static char buf[64];
        snprintf(buf, sizeof(buf), "Injecting in %.0fs...", runTimer_);
        return buf;
    }
    case RS_Finalizing:    return "Injecting...";
    case RS_CheckingCrash: return "Verifying...";
    case RS_Finished:      return "Done!";
    default:
        return "Run " + product_->GetTitle();
    }
}

bool ProductView::IsButtonDisabled() const
{
    return runState_ != RS_Idle;
}

void ProductView::DrawBackButton(ImDrawList* dl, const ImVec2& pos,
    float size, float alpha)
{
    ImU32 col = IM_COL32(220, 220, 230, (int)(255 * alpha));
    float m = size * 0.25f;
    float cx = pos.x + size * 0.5f;
    float cy = pos.y + size * 0.5f;

    dl->AddLine(
        ImVec2(cx + m * 0.3f, cy - m),
        ImVec2(cx - m * 0.7f, cy),
        col, 2.0f);
    dl->AddLine(
        ImVec2(cx - m * 0.7f, cy),
        ImVec2(cx + m * 0.3f, cy + m),
        col, 2.0f);
}

void ProductView::Draw(const ImVec2& windowSize, ImU32 accentColor,
    ImU32 accentColorSub)
{
    if (!IsActive() || !product_) return;

    const float t = anim_;
    const float eased = t * t * (3.0f - 2.0f * t);
    const float cr = ImGui::GetStyle().WindowRounding;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(windowSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, cr);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));

    ImGui::Begin("##ProductView", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 wp = ImGui::GetWindowPos();
    const ImVec2 p0 = wp;
    const ImVec2 p1 = ImVec2(wp.x + windowSize.x, wp.y + windowSize.y);

    ID3D11ShaderResourceView* headerSrv = product_->GetHeader();
    if (!headerSrv) headerSrv = product_->GetCurrentPoster();

    if (headerSrv)
    {
        UINT blurW = (UINT)ImMax(windowSize.x / 4.0f, 1.0f);
        UINT blurH = (UINT)ImMax(windowSize.y / 4.0f, 1.0f);

        std::string cacheKey = "pv_bg_" + product_->GetHash();
        ID3D11ShaderResourceView* blurred = NemesisBlur::GetBlurredTexture(
            cacheKey.c_str(), headerSrv,
            blurW, blurH, 3, 1.5f, bgDirty_);
        bgDirty_ = false;

        if (blurred)
        {
            dl->AddImageRounded(
                (ImTextureID)blurred, p0, p1,
                ImVec2(0, 0), ImVec2(1, 1),
                IM_COL32(255, 255, 255, (int)(255 * eased)),
                cr);
        }
    }

    dl->AddRectFilled(p0, p1,
        IM_COL32(10, 10, 12, (int)(255 * eased)), cr);

    const float pad = 28.0f;
    const float topBar = 48.0f;

    const float backBtnSize = 32.0f;
    const float backX = pad * 0.5f;
    const float backY = (topBar - backBtnSize) * 0.5f;

    ImGui::SetCursorPos(ImVec2(backX, backY));
    if (ImGui::InvisibleButton("##pv_back", ImVec2(backBtnSize, backBtnSize)))
    {
        Close();
    }

    bool backHovered = ImGui::IsItemHovered();
    if (backHovered)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    ImVec2 backScreenPos(wp.x + backX, wp.y + backY);
    DrawBackButton(dl, backScreenPos, backBtnSize, eased * (backHovered ? 1.0f : 0.7f));

    if (backHovered)
    {
        float uw = backBtnSize * 0.6f;
        float ux = backScreenPos.x + (backBtnSize - uw) * 0.5f;
        float uy = backScreenPos.y + backBtnSize + 2.0f;
        dl->AddRectFilled(
            ImVec2(ux, uy), ImVec2(ux + uw, uy + 2.0f),
            accentColor, 1.0f);
    }

    const float posterW = windowSize.x * 0.35f;
    const float posterH = windowSize.y - topBar - pad * 2.0f;
    const ImVec2 posterPos(wp.x + pad, wp.y + topBar + pad);
    const ImVec2 posterEnd(posterPos.x + posterW, posterPos.y + posterH);

    ID3D11ShaderResourceView* posterSrv = product_->GetCurrentPoster();
    if (posterSrv)
    {
        dl->AddImageRounded(
            (ImTextureID)posterSrv,
            posterPos, posterEnd,
            ImVec2(0, 0), ImVec2(1, 1),
            IM_COL32(255, 255, 255, (int)(255 * eased)),
            10.0f);
    }
    else
    {
        dl->AddRectFilled(posterPos, posterEnd,
            IM_COL32(40, 40, 45, (int)(255 * eased)), 10.0f);
    }

    const float infoX = posterEnd.x + pad;
    const float infoW = windowSize.x - (infoX - wp.x) - pad;
    float infoY = wp.y + topBar + pad;

    ImFont* font = ImGui::GetFont();
    const float titleFontSize = ImGui::GetFontSize() * 1.8f;
    const float descFontSize = ImGui::GetFontSize() * 1.0f;
    const float labelFontSize = ImGui::GetFontSize() * 0.9f;

    // Title
    dl->AddText(font, titleFontSize,
        ImVec2(infoX, infoY),
        IM_COL32(255, 255, 255, (int)(255 * eased)),
        product_->GetTitle().c_str());
    infoY += titleFontSize + 8.0f;

    // Separator
    dl->AddLine(
        ImVec2(infoX, infoY),
        ImVec2(infoX + infoW, infoY),
        IM_COL32(80, 80, 85, (int)(180 * eased)), 1.0f);
    infoY += 16.0f;

    // Description
    dl->AddText(font, descFontSize,
        ImVec2(infoX, infoY),
        IM_COL32(180, 180, 190, (int)(255 * eased)),
        "No description available.");
    infoY += descFontSize + 24.0f;

    // Info rows
    auto drawInfoRow = [&](const char* label, const char* value) {
        dl->AddText(font, labelFontSize,
            ImVec2(infoX, infoY),
            IM_COL32(140, 140, 150, (int)(220 * eased)),
            label);

        ImVec2 labelSz = font->CalcTextSizeA(labelFontSize, FLT_MAX, 0, label);
        dl->AddText(font, labelFontSize,
            ImVec2(infoX + labelSz.x + 12.0f, infoY),
            IM_COL32(220, 220, 230, (int)(255 * eased)),
            value);
        infoY += labelFontSize + 6.0f;
        };

    drawInfoRow("Status:",
        product_->GetStatus() == SS_Ready ? "Online" :
        product_->GetStatus() == SS_InMaintenance ? "Maintenance" :
        product_->GetStatus() == SS_ComingSoon ? "Coming Soon" : "Offline");

    char steamIdBuf[32];
    snprintf(steamIdBuf, sizeof(steamIdBuf), "%d", product_->GetSteamId());
    drawInfoRow("Steam ID:", steamIdBuf);

    drawInfoRow("Process:", product_->GetProcName().c_str());

    char injectTimeBuf[32];
    snprintf(injectTimeBuf, sizeof(injectTimeBuf), "%ds",
        product_->GetDefaultInjectTime());
    drawInfoRow("Inject delay:", injectTimeBuf);

    // ====== Run button (bottom right) ======
    const float btnW = infoW;
    const float btnH = 38.0f;
    const float btnLocalX = infoX - wp.x;
    const float btnLocalY = windowSize.y - pad - btnH;

    ImGui::SetCursorPos(ImVec2(btnLocalX, btnLocalY));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

    bool disabled = IsButtonDisabled();
    std::string btnText = GetButtonText();

    ImVec4 btnCol(67.f / 255.f, 120.f / 255.f, 232.f / 255.f, eased);
    if (disabled) btnCol = ImVec4(0.22f, 0.22f, 0.25f, eased);

    ImGui::PushStyleColor(ImGuiCol_Button, btnCol);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        ImVec4(btnCol.x + 0.05f, btnCol.y + 0.05f, btnCol.z + 0.05f, eased));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
        ImVec4(btnCol.x - 0.05f, btnCol.y - 0.05f, btnCol.z - 0.05f, eased));
    ImGui::PushStyleColor(ImGuiCol_Text,
        ImVec4(0.93f, 0.95f, 0.98f, eased));

    if (ImGui::Button(btnText.c_str(), ImVec2(btnW, btnH)) && !disabled)
    {
        runState_ = RS_ClosingSteam;
        runTimer_ = 0.5f;
    }

    if (ImGui::IsItemHovered() && !disabled)
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(3);
}