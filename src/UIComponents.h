#ifndef UICOMPONENTS_H
#define UICOMPONENTS_H

#include "AppState.h"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

using namespace ftxui;

class UIComponents {
public:
    UIComponents(AppState& state, ScreenInteractive& screen);
    
    // Create UI components
    void CreateComponents();
    
    // Get the main container
    Component GetMainContainer();
    
    // Get specific components for event handling
    Component GetDeleteMenu();
    Component GetPickerMenu();
    
    // Render different views
    Element RenderLibraryView();
    Element RenderReaderView();
    Element RenderFilePickerView();
    Element RenderShowMessageView();
    Element RenderLoadingView();
    Element RenderTableOfContentsView();
    Element RenderConfirmOcrView();
    Element RenderDeleteConfirmView();
    Element RenderSystemInfoView();
    
private:
    AppState& app_state_;
    ScreenInteractive& screen_;
    
    // UI Components
    Component library_menu_;
    Component picker_menu_;
    Component toc_menu_;
    Component ok_button_;
    Component confirm_ocr_container_;
    Component delete_menu_;
    Component delete_confirm_renderer_;
    Component main_container_;
    Component modal_component_;
    Component root_container_;
};

#endif // UICOMPONENTS_H