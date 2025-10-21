/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

class  QOpenGLShaderProgram; 
class DebugRenderer : protected QOpenGLFunctions_4_0_Core {
public:
	DebugRenderer();

	bool Init();

	// viewport = (x, y, width, height)
	void Render(const glm::mat4& cameraMat, const glm::mat4& projMat,
                const glm::vec4& viewport, const glm::vec2& nearFar);

	// call at end of frame.
	void EndFrame();

	void Line(const glm::vec3& start, const glm::vec3& end, const glm::vec3& color);
	void Transform(const glm::mat4& m, float axisLen = 1.0f);

protected:
    std::unique_ptr<QOpenGLShaderProgram> ddProg;
	//std::shared_ptr<Program> ddProg;
	std::vector<glm::vec3> linePositionVec;
	std::vector<glm::vec3> lineColorVec;
};


