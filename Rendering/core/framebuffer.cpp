/*
    Copyright (c) 2024 Anthony J. Thibault
    This software is licensed under the MIT License. See LICENSE for more details.
*/

#include "framebuffer.h" 
#ifdef __ANDROID__
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#else
#include <QtOpenGL/QOpenGLFunctions_4_5_Core> 
#endif 

#include "texture.h"

FrameBuffer::FrameBuffer()
{
    glFuncs = QOpenGLContext::currentContext()->extraFunctions();
    if (!glFuncs) {
        throw std::runtime_error("Failed to get OpenGL functions");
    }  
    glFuncs->glGenFramebuffers(1, &fbo);
}

FrameBuffer::~FrameBuffer()
{
    glFuncs->glDeleteFramebuffers(1, &fbo);
    fbo = 0;
}

void FrameBuffer::Bind() const
{
    glFuncs->glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

void FrameBuffer::AttachColor(std::shared_ptr<Texture> colorTex)
{
    Bind();
    glFuncs->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex->texture, 0);
    colorAttachment = colorTex;
}

void FrameBuffer::AttachDepth(std::shared_ptr<Texture> depthTex)
{
    Bind();
    glFuncs->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex->texture, 0);
    depthAttachment = depthTex;
}

void FrameBuffer::AttachStencil(std::shared_ptr<Texture> stencilTex)
{
    Bind();
    glFuncs->glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, stencilTex->texture, 0);
    stencilAttachment = stencilTex;
}

bool FrameBuffer::IsComplete() const
{
    return glFuncs->glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}
