#pragma once

// =============================================================================
//  UsedHook.hpp - СПРАВКА ПО ИСПОЛЬЗОВАНИЮ DX11 OVERLAY HOOK (RenderHook)
//
//  Этот файл НИЧЕГО НЕ ДЕЛАЕТ. Только примеры и описание API.
//  Реальная реализация - в RenderHook.cpp.
// =============================================================================
//
//  1) ЗАПУСК / ОСТАНОВКА
//  ---------------------------------------------------------------------------
//  В dllmain (Bootstrap-поток), после загрузки d3d11.dll:
//
//      #include "RenderDrx11/RenderHook.hpp"
//      Nemesis::RenderHook::Start();   // kiero locate -> MinHook хук Present -> ImGui
//      ...
//      Nemesis::RenderHook::Stop();    // снять хук + вернуть WndProc
//
//  Тоггл меню по умолчанию - клавиша INSERT (см. WndProc в RenderHook.cpp).
//
//
//  2) КАК ДОБАВИТЬ СВОЙ РЕНДЕР В ОВЕРЛЕЙ
//  ---------------------------------------------------------------------------
//  Рисуем внутри функции DrawMenu() в RenderHook.cpp (вызывается каждый кадр,
//  пока меню открыто). Пример содержимого DrawMenu:
//
//      ImGui::Begin("Nemesis Loader");
//      static bool thirdperson = false;
//      if (ImGui::Checkbox("Thirdperson", &thirdperson))
//          Nemesis::CameraPositionChange::Toggle();
//      ImGui::SliderInt("PaintKit", &g_paint, 0, 1000);
//      ImGui::End();
//
//  ВАЖНО: вызовы ImGui допустимы только между ImGui::NewFrame() и
//  ImGui::Render() - то есть только из DrawMenu(), не из других потоков.
//
//
//  3) ОТРИСОВКА ВСЕГДА (ESP), А НЕ ТОЛЬКО ПРИ ОТКРЫТОМ МЕНЮ
//  ---------------------------------------------------------------------------
//  Для ESP/оверлея, который виден всегда, рендерить кадр нужно безусловно,
//  а не только при g_menuOpen. Схема (псевдокод для HookPresent):
//
//      ImGui_ImplDX11_NewFrame();
//      ImGui_ImplWin32_NewFrame();
//      ImGui::NewFrame();
//      ImDrawList* bg = ImGui::GetBackgroundDrawList();
//      bg->AddRect(p0, p1, IM_COL32(0,255,0,255));   // бокс ESP
//      if (g_menuOpen) DrawMenu();
//      ImGui::Render();
//      ...RenderDrawData...
//
//
//  4) КАК ПОВЕСИТЬ СВОЙ ХУК ЧЕРЕЗ MinHook (общий шаблон)
//  ---------------------------------------------------------------------------
//      using Fn = ReturnT (__fastcall*)(Args...);
//      static Fn  oOriginal = nullptr;
//      static ReturnT __fastcall Detour(Args... a) {
//          // ... своя логика ...
//          return oOriginal(a...);
//      }
//      // установка:
//      MH_Initialize();                          // один раз на процесс
//      MH_CreateHook(targetAddr, &Detour, reinterpret_cast<void**>(&oOriginal));
//      MH_EnableHook(targetAddr);
//      // снятие:
//      MH_DisableHook(targetAddr);
//
//
//  5) ИНДЕКСЫ VTABLE ДЛЯ kiero2 (D3D11Output)
//  ---------------------------------------------------------------------------
//      output.swapchain_methods[8]   -> IDXGISwapChain::Present
//      output.swapchain_methods[13]  -> IDXGISwapChain::ResizeBuffers
//      output.device_methods[...]    -> ID3D11Device
//      output.context_methods[...]   -> ID3D11DeviceContext
//
//  При ResizeBuffers нужно пересоздавать RenderTargetView (g_rtv) - иначе
//  после смены разрешения оверлей сломается. (В текущей версии не обрабатывается.)
// =============================================================================
