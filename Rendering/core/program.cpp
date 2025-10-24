/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/
#define GL_GLEXT_PROTOTYPES 1
#include "program.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <sstream> 
#include <qopenglext.h>
#include<QtGui/qopenglfunctions.h>
 #include <QtGui/qopenglcontext.h>
//#include "log.h"
#include "util.h"

#ifndef NDEBUG
#define WARNINGS_AS_ERRORS
#endif

static std::string ExpandMacros(std::vector<std::pair<std::string, std::string>> macros, const std::string& source)
{
    std::string result = source;

    for (const auto& macro : macros)
    {
        std::string::size_type pos = 0;
        while ((pos = result.find(macro.first, pos)) != std::string::npos)
        {
            result.replace(pos, macro.first.length(), macro.second);
            // Move past the last replaced position
            pos += macro.second.length();
        }
    }

    return result;
}

static void DumpShaderSource(const std::string& source)
{
    std::stringstream ss(source);
    std::string line;
    int i = 1;
    /*while (std::getline(ss, line))
    {
        SPDLOG_INFO("%04d: %s\n", i, line.c_str());
        i++;
    }
    SPDLOG_INFO("\n");*/
}

static bool CompileShader(GLenum type, const std::string& source, GLint* shaderOut, const std::string& debugName) {
  QOpenGLFunctions* glFuncs = QOpenGLContext::currentContext()->functions(); 
    GLint shader = glFuncs->glCreateShader(type);
    int size = static_cast<int>(source.size());
    const GLchar* sourcePtr = source.c_str();
    glFuncs->glShaderSource(shader, 1, (const GLchar**)&sourcePtr, &size);
    glFuncs->glCompileShader(shader);

    GLint compiled;
    glFuncs->glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled)
    {
        //SPDLOG_ERROR("shader compilation error for \"%s\"!\n", debugName.c_str());
    }

    GLint bufferLen = 0;
    glFuncs->glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &bufferLen);
    if (bufferLen > 1)
    {
        if (compiled)
        {
         //   SPDLOG_ERROR("shader compilation warning for \"%s\"!\n", debugName.c_str());
        }

        GLsizei len = 0;
        std::unique_ptr<char> buffer(new char[bufferLen]);
        glFuncs->glGetShaderInfoLog(shader, bufferLen, &len, buffer.get());
        SPDLOG_ERROR("%s\n", buffer.get());
        DumpShaderSource(source);
    }

#ifdef WARNINGS_AS_ERRORS
    if (!compiled || bufferLen > 1)
#else
    if (!compiled)
#endif
    {
        return false;
    }

    *shaderOut = shader;
    return true;
}

Program::Program() : program(0), vertShader(0), geomShader(0), fragShader(0), computeShader(0)
{
#ifdef __ANDROID__
    AddMacro("HEADER", "#version 320 es\nprecision highp float;");
#else
    AddMacro("HEADER", "#version 460");
#endif
    glFuncs = QOpenGLContext::currentContext()->functions();
}

Program::~Program()
{
    Delete();
}

void Program::AddMacro(const std::string& key, const std::string& value)
{
    // In order to keep the glsl code compiling if the macro is not applied.
    // the key is enclosed in a c-style comment and double %.
    std::string token = "/*%%" + key + "%%*/";
    macros.push_back(std::pair(token, value));
}

bool Program::LoadVertFrag(const std::string& vertFilename, const std::string& fragFilename)
{
    return LoadVertGeomFrag(vertFilename, std::string(), fragFilename);
}

bool Program::LoadVertGeomFrag(const std::string& vertFilename, const std::string& geomFilename, const std::string& fragFilename)
{
    // Delete old shader/program
    Delete();

    const bool useGeomShader = !geomFilename.empty();

    if (useGeomShader)
    {
        debugName = vertFilename + " + " + geomFilename + " + " + fragFilename;
    }
    else
    {
        debugName = vertFilename + " + " + fragFilename;
    }

    std::string vertSource, geomSource, fragSource;
    if (!LoadFile(vertFilename, vertSource))
    {
        SPDLOG_ERROR("Failed to load vertex shader %s\n", vertFilename.c_str());
        return false;
    }
    vertSource = ExpandMacros(macros, vertSource);

    if (useGeomShader)
    {
        if (!LoadFile(geomFilename, geomSource))
        {
            SPDLOG_ERROR("Failed to load geometry shader %s\n", geomFilename.c_str());
            return false;
        }
        geomSource = ExpandMacros(macros, geomSource);
    }

    if (!LoadFile(fragFilename, fragSource))
    {
        SPDLOG_ERROR("Failed to load fragment shader \"%s\"\n", fragFilename.c_str());
        return false;
    }
    fragSource = ExpandMacros(macros, fragSource);

    if (!CompileShader(GL_VERTEX_SHADER, vertSource, &vertShader, vertFilename))
    {
        SPDLOG_ERROR("Failed to compile vertex shader \"%s\"\n", vertFilename.c_str());
        return false;
    }

    if (useGeomShader)
    {
        geomSource = ExpandMacros(macros, geomSource);
        if (!CompileShader(GL_GEOMETRY_SHADER, geomSource, &geomShader, geomFilename))
        {
            SPDLOG_ERROR("Failed to compile geometry shader \"%s\"\n", geomFilename.c_str());
            return false;
        }
    }

    if (!CompileShader(GL_FRAGMENT_SHADER, fragSource, &fragShader, fragFilename))
    {
        SPDLOG_ERROR("Failed to compile fragment shader \"%s\"\n", fragFilename.c_str());
        return false;
    }
    
    program = glFuncs->glCreateProgram();
    glFuncs->glAttachShader(program, vertShader);
    glFuncs->glAttachShader(program, fragShader);
    if (useGeomShader)
    {
        glFuncs->glAttachShader(program, geomShader);
    }
    glFuncs->glLinkProgram(program);

    if (!CheckLinkStatus())
    {
        SPDLOG_ERROR("Failed to link program \"%s\"\n", debugName.c_str());

        // dump shader source for reference
        SPDLOG_INFO("\n");
        SPDLOG_INFO("%s =\n", vertFilename.c_str());
        DumpShaderSource(vertSource);
        if (useGeomShader)
        {
            SPDLOG_INFO("%s =\n", geomFilename.c_str());
            DumpShaderSource(geomSource);
        }
        SPDLOG_INFO("%s =\n", fragFilename.c_str());
        DumpShaderSource(fragSource);

        return false;
    }

    const int MAX_NAME_SIZE = 1028;
    static char name[MAX_NAME_SIZE];

    GLint numAttribs;
    glFuncs->glGetProgramiv(program, GL_ACTIVE_ATTRIBUTES, &numAttribs);
    for (int i = 0; i < numAttribs; ++i)
    {
        Variable v;
        GLsizei strLen;
        glFuncs->glGetActiveAttrib(program, i, MAX_NAME_SIZE, &strLen, &v.size, &v.type, name);
        v.loc = glFuncs->glGetAttribLocation(program, name);
        attribs[name] = v;
    }

    GLint numUniforms;
    glFuncs->glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &numUniforms);
    for (int i = 0; i < numUniforms; ++i)
    {
        Variable v;
        GLsizei strLen;
        glFuncs->glGetActiveUniform(program, i, MAX_NAME_SIZE, &strLen, &v.size, &v.type, name);
        int loc = glFuncs->glGetUniformLocation(program, name);
        v.loc = loc;
        uniforms[name] = v;
    }

    return true;
}

bool Program::LoadCompute(const std::string& computeFilename)
{
    // Delete old shader/program
    Delete();

    debugName = computeFilename;

    GL_ERROR_CHECK("Program::LoadCompute begin");

    std::string computeSource;
    if (!LoadFile(computeFilename, computeSource))
    {
        SPDLOG_ERROR("Failed to load compute shader \"%s\"\n", computeFilename.c_str());
        return false;
    }

    GL_ERROR_CHECK("Program::LoadCompute LoadFile");

    computeSource = ExpandMacros(macros, computeSource);
    if (!CompileShader(GL_COMPUTE_SHADER, computeSource, &computeShader, computeFilename))
    {
        SPDLOG_ERROR("Failed to compile compute shader \"%s\"\n", computeFilename.c_str());
        return false;
    }

    GL_ERROR_CHECK("Program::LoadCompute CompileShader");

    program = glFuncs->glCreateProgram();
    glFuncs->glAttachShader(program, computeShader);
    glFuncs->glLinkProgram(program);

    GL_ERROR_CHECK("Program::LoadCompute Attach and Link");

    if (!CheckLinkStatus())
    {
        SPDLOG_ERROR("Failed to link program \"%s\"\n", debugName.c_str());

        // dump shader source for reference
        SPDLOG_INFO("\n");
        SPDLOG_INFO("%s =\n", computeFilename.c_str());
        DumpShaderSource(computeSource);

        return false;
    }

    const int MAX_NAME_SIZE = 1028;
    static char name[MAX_NAME_SIZE];

    GLint numUniforms;
    glFuncs->glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &numUniforms);
    for (int i = 0; i < numUniforms; ++i)
    {
        Variable v;
        GLsizei strLen;
        glFuncs->glGetActiveUniform(program, i, MAX_NAME_SIZE, &strLen, &v.size, &v.type, name);
        int loc = glFuncs->glGetUniformLocation(program, name);
        v.loc = loc;
        uniforms[name] = v;
    }

    GL_ERROR_CHECK("Program::LoadCompute get uniforms");

    // TODO: build reflection info on shader storage blocks

    return true;
}

void Program::Bind() const
{
   glFuncs-> glUseProgram(program);
}

int Program::GetUniformLoc(const std::string& name) const
{
    auto iter = uniforms.find(name);
    if (iter != uniforms.end())
    {
        return iter->second.loc;
    }
    else
    {
        assert(false);
        SPDLOG_INFO("Could not find uniform \"%s\" for program \"%s\"\n", name.c_str(), debugName.c_str());
        return 0;
    }
}

int Program::GetAttribLoc(const std::string& name) const
{
    auto iter = attribs.find(name);
    if (iter != attribs.end())
    {
        return iter->second.loc;
    }
    else
    {
        SPDLOG_INFO("Could not find attrib \"%s\" for program \"%s\"\n", name.c_str(), debugName.c_str());
        assert(false);
        return 0;
    }
}

void Program::SetUniformRaw(int loc, uint32_t value) const
{
//    glFuncs->glUniform1ui(loc, value);
}

void Program::SetUniformRaw(int loc, int32_t value) const
{
    glFuncs->glUniform1i(loc, value);
}

void Program::SetUniformRaw(int loc, float value) const
{
    glFuncs->glUniform1f(loc, value);
}

void Program::SetUniformRaw(int loc, const glm::vec2& value) const
{
    glFuncs->glUniform2fv(loc, 1, (float*)&value);
}

void Program::SetUniformRaw(int loc, const glm::vec3& value) const
{
  glFuncs->glUniform3fv(loc, 1, (float*)&value);
}

void Program::SetUniformRaw(int loc, const glm::vec4& value) const
{
  glFuncs->glUniform4fv(loc, 1, (float*)&value);
}

void Program::SetUniformRaw(int loc, const glm::mat2& value) const
{
  glFuncs->glUniformMatrix2fv(loc, 1, GL_FALSE, (float*)&value);
}

void Program::SetUniformRaw(int loc, const glm::mat3& value) const
{
  glFuncs->glUniformMatrix3fv(loc, 1, GL_FALSE, (float*)&value);
}

void Program::SetUniformRaw(int loc, const glm::mat4& value) const
{
  glFuncs->glUniformMatrix4fv(loc, 1, GL_FALSE, (float*)&value);
}

void Program::SetAttribRaw(int loc, float* values, size_t stride) const
{
  glFuncs->glVertexAttribPointer(loc, 1, GL_FLOAT, GL_FALSE, (GLsizei)stride, values);
  glFuncs->glEnableVertexAttribArray(loc);
}

void Program::SetAttribRaw(int loc, glm::vec2* values, size_t stride) const
{
  glFuncs->glVertexAttribPointer(loc, 2, GL_FLOAT, GL_FALSE, (GLsizei)stride, values);
  glFuncs->glEnableVertexAttribArray(loc);
}

void Program::SetAttribRaw(int loc, glm::vec3* values, size_t stride) const
{
  glFuncs->glVertexAttribPointer(loc, 3, GL_FLOAT, GL_FALSE, (GLsizei)stride, values);
  glFuncs->glEnableVertexAttribArray(loc);
}

void Program::SetAttribRaw(int loc, glm::vec4* values, size_t stride) const
{
  glFuncs->glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, (GLsizei)stride, values);
  glFuncs->glEnableVertexAttribArray(loc);
}

void Program::Delete()
{
    debugName = "";

    if (vertShader > 0)
    {
      glFuncs->glDeleteShader(vertShader);
        vertShader = 0;
    }

    if (geomShader > 0)
    {
      glFuncs->glDeleteShader(geomShader);
        geomShader = 0;
    }

    if (fragShader > 0)
    {
      glFuncs->glDeleteShader(fragShader);
        fragShader = 0;
    }

    if (computeShader > 0)
    {
      glFuncs->glDeleteShader(computeShader);
        computeShader = 0;
    }

    if (program > 0)
    {
      glFuncs->glDeleteProgram(program);
        program = 0;
    }

    uniforms.clear();
    attribs.clear();
}

bool Program::CheckLinkStatus()
{
    GLint linked;
  glFuncs->glGetProgramiv(program, GL_LINK_STATUS, &linked);

    if (!linked)
    {
        SPDLOG_ERROR("Failed to link shaders \"%s\"\n", debugName.c_str());
    }

    const GLint MAX_BUFFER_LEN = 4096;
    GLsizei bufferLen = 0;
    std::unique_ptr<char> buffer(new char[MAX_BUFFER_LEN]);
    glFuncs->glGetProgramInfoLog(program, MAX_BUFFER_LEN, &bufferLen, buffer.get());
    if (bufferLen > 0)
    {
        if (linked)
        {
            SPDLOG_INFO("Warning during linking shaders \"%s\"\n", debugName.c_str());
        }
        SPDLOG_INFO("%s\n", buffer.get());
    }

#ifdef WARNINGS_AS_ERRORS
    if (!linked || bufferLen > 1)
#else
    if (!linked)
#endif
    {
        return false;
    }

    return true;
}
