//
// Created by Federico Andrucci on 22/08/23.
//
#pragma once

#ifndef INPUT_H
#define INPUT_H

#include "Application.h"
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

enum class KeyCode {
    Space = 32,
    Escape = 256,
    Enter = 257,
    W = 87,
    A = 65,
    S = 83,
    D = 68,
};

enum class MouseButton {
    Left = 0,
    Right = 1,
    Middle = 2,
};

enum class CursorMode {
    Normal = 0,
    Hidden = 1,
    Locked = 2
};

class Input {
public:
    static bool IsKeyDown(KeyCode keycode);
    static bool IsMouseButtonDown(MouseButton button);
    static glm::vec2 GetMousePosition();
    static void SetCursorMode(CursorMode cursorMode);
};


#endif // INPUT_H
