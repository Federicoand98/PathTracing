//
// Created by Federico Andrucci on 22/08/23.
//

#include "Input.h"

namespace PathTracer {

	bool Input::IsKeyDown(KeyCode keycode) {
		GLFWwindow* window = Application::Get().GetWindow();
		int state = glfwGetKey(window, (int)keycode);

		return state == GLFW_PRESS || state == GLFW_REPEAT;
	}

	bool Input::IsMouseButtonDown(MouseButton button) {
		GLFWwindow* window = Application::Get().GetWindow();
		int state = glfwGetMouseButton(window, (int)button);

		return state == GLFW_PRESS;
	}

	glm::vec2 Input::GetMousePosition() {
		double xPos, yPos;
		GLFWwindow* window = Application::Get().GetWindow();
		glfwGetCursorPos(window, &xPos, &yPos);

		return {xPos, yPos};
	}

	void Input::SetCursorMode(CursorMode cursorMode) {
		GLFWwindow* window = Application::Get().GetWindow();
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL + (int)cursorMode);
	}
}
