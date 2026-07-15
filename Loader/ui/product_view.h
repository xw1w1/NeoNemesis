#pragma once

#include <string>
#include <d3d11.h>
#include "imgui.h"
#include "../subscription/product.h"
#include "../injector/infector.h"

class ProductView
{
public:
    enum eRunState {
        RS_Idle,
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

    void Open(Product* product);

    void Close();

    bool IsActive() const;

    bool IsFullyOpen() const;

    bool BlocksInput() const;

    void Update(float deltaTime);

    void Draw(const ImVec2& windowSize, ImU32 accentColor, ImU32 accentColorSub);

    Product* GetProduct() const { return product_; }

    bool IsAppDone() const { return appDone_; }

private:
    void DrawBackButton(ImDrawList* dl, const ImVec2& pos, float size, float alpha);
    void UpdateRunState(float dt);
    std::string GetButtonText() const;
    bool IsButtonDisabled() const;

    Product* product_ = nullptr;
    float      anim_ = 0.0f;
    bool       open_ = false;
    bool       bgDirty_ = true;
    bool       appDone_ = false;

    eRunState  runState_ = RS_Idle;
    float      runTimer_ = 0.0f;
    HINFRES    injectStatus_ = HINFRES::EMPTY;
};