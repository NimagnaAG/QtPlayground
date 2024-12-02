#include "pch.h"

#include "OpenGlWidget.h"

#include <QtCore/QRandomGenerator>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMessageBox>
#include <QtGui/QImage>
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <Rendering/tiny_gltf.h>

namespace nimagna {

OpenGlWidget::OpenGlWidget(QWidget* parent /*= nullptr*/, Qt::WindowFlags f /*= Qt::WindowFlags()*/)
    : QOpenGLWidget(parent, f) {
  mTextureRenderObject =
      std::make_unique<TextureRenderObject>(TextureRenderObject::kDefaultTextureTarget);
  QSurfaceFormat format;
  format.setRenderableType(QSurfaceFormat::OpenGL);
  format.setProfile(QSurfaceFormat::CoreProfile);
  format.setVersion(4, 0);
  format.setSamples(8);
  setFormat(format);
  setMouseTracking(true);
  mTransformMatrix.setToIdentity();
}

void OpenGlWidget::processModel(const tinygltf::Model& model) {
  initializeOpenGLFunctions();

  for (const auto& mesh : model.meshes) {
    for (const auto& primitive : mesh.primitives) {
      // Create and bind Vertex Array Object (VAO)
      auto vao = std::make_unique<QOpenGLVertexArrayObject>();
      if (!vao->create()) {
        SPDLOG_ERROR("Failed to create VertexArrayObject");
        continue;
      }
      vao->bind();

      // Create and bind Vertex Buffer Object (VBO)
      QOpenGLBuffer vbo(QOpenGLBuffer::VertexBuffer);
      if (!vbo.create()) {
        SPDLOG_ERROR("Failed to create VertexBufferObject");
        continue;
      }
      vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);

      // Assuming the primitive has positions
      const tinygltf::Accessor& posAccessor =
          model.accessors[primitive.attributes.find("POSITION")->second];
      const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
      const tinygltf::Buffer& posBuffer = model.buffers[posView.buffer];

      // Copy and scale vertex positions
      std::vector<float> scaledPositions(posAccessor.count * 3);  // Assuming vec3 positions
      const float* positions = reinterpret_cast<const float*>(&posBuffer.data[posView.byteOffset]);
      float scaleFactor = 10.0f;  // Scale factor

      for (size_t i = 0; i < posAccessor.count; ++i) {
        scaledPositions[i * 3 + 0] = positions[i * 3 + 0] * scaleFactor;
        scaledPositions[i * 3 + 1] = positions[i * 3 + 1] * scaleFactor;
        scaledPositions[i * 3 + 2] = positions[i * 3 + 2] * scaleFactor;
      }

      vbo.bind();
      vbo.allocate(scaledPositions.data(),
                   static_cast<int>(scaledPositions.size() * sizeof(float)));

      // Set up vertex attribute pointers
      glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
      glEnableVertexAttribArray(0);

      // Create and bind Index Buffer Object (EBO) if it exists
      QOpenGLBuffer ebo(QOpenGLBuffer::IndexBuffer);
      int indexCount = 0;
      if (primitive.indices >= 0) {
        const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
        const tinygltf::BufferView& indexView = model.bufferViews[indexAccessor.bufferView];
        const tinygltf::Buffer& indexBuffer = model.buffers[indexView.buffer];

        if (!ebo.create()) {
          SPDLOG_ERROR("Failed to create IndexBufferObject");
          continue;
        }
        ebo.setUsagePattern(QOpenGLBuffer::StaticDraw);
        ebo.bind();
        ebo.allocate(&indexBuffer.data[indexView.byteOffset],
                     static_cast<int>(indexView.byteLength));

        indexCount = static_cast<int>(indexAccessor.count);  // Store the index count
      }

      // Unbind VAO
      vao->release();

      // Store the VAO and index count for rendering later
      mVAOs.push_back(std::move(vao));
      mIndexCounts.push_back(indexCount);
    }
  }
}

void OpenGlWidget::loadTexture(const QString& filePath) {
  QImage image(filePath);
  if (image.isNull()) {
    SPDLOG_ERROR("Failed to load texture image from file: {}", filePath.toStdString());
    return;
  }

  // Convert the image to the format expected by OpenGL
  QImage glImage = image.convertToFormat(QImage::Format_RGBA8888);
  mTextureRenderObject->setTextureData(glImage);

  /*
  GLuint textureID;
  glGenTextures(1, &textureID);
  glBindTexture(GL_TEXTURE_2D, textureID);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, glImage.width(), glImage.height(), 0, GL_RGBA,
               GL_UNSIGNED_BYTE, glImage.bits());

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glBindTexture(GL_TEXTURE_2D, 0);  // Unbind the texture
  return 1;
  */

}

void OpenGlWidget::renderModel() {
  if (!mTextureRenderObject || !mTextureRenderObject->mShaderProgram) {
    SPDLOG_ERROR("Shader program is not available.");
    return;
  }

  // Use the shader program from TextureRenderObject
  mTextureRenderObject->mShaderProgram->bind();

  // Pass the transformation matrix to the shader
  GLuint transformLoc = mTextureRenderObject->mShaderProgram->uniformLocation("worldToView");
  if (transformLoc == -1) {
    SPDLOG_ERROR("Failed to get uniform location for transform.");
    return;
  }
  mTextureRenderObject->mShaderProgram->setUniformValue(transformLoc, mTransformMatrix);
  loadTexture("D:\\3dassets\\avocado\\Avocado_baseColor.png");

  // Load and bind the new texture
  /*
  GLuint newTextureID = loadTexture("D:\\3dassets\\avocado\\Avocado_baseColor.png");
  if (newTextureID == 0) {
    SPDLOG_ERROR("Failed to load new texture.");
    return;
  }
  */

  glActiveTexture(GL_TEXTURE0 + mTextureRenderObject->mColorTextureUnit);  // Activate the texture unit

  // Set the texture uniform in the shader
  GLuint textureLoc = mTextureRenderObject->mShaderProgram->uniformLocation("imageTexture");
  if (textureLoc == -1) {
    SPDLOG_ERROR("Failed to get uniform location for texture.");
    return;
  }
  mTextureRenderObject->mShaderProgram->setUniformValue(textureLoc, mTextureRenderObject->mColorTextureUnit);

  for (size_t i = 0; i < mVAOs.size(); ++i) {
    mVAOs[i]->bind();
    glDrawElements(GL_TRIANGLES, mIndexCounts[i], GL_UNSIGNED_SHORT, 0);
    mVAOs[i]->release();
  }

  mTextureRenderObject->mShaderProgram->release();
}

OpenGlWidget::~OpenGlWidget() {
  // the texture render object needs a current context for destruction
  context()->makeCurrent(context()->surface());
  mTextureRenderObject.reset();
}

void OpenGlWidget::setRenderer(std::shared_ptr<Renderer> renderer) {
  if (mRenderer) {
    disconnect(mRenderer.get(), nullptr, this, nullptr);
  }
  mRenderer = std::move(renderer);
  if (mRenderer) {
    connect(mRenderer.get(), &Renderer::renderFrameUpdated, this, [this]() { update(); });
  }
}

void OpenGlWidget::enableTrackball(bool enabled) {
  if (enabled) {
    // trackball turned on: remember current 3D framing
    mOriginalTrackballFraming = mRenderer->renderObjectManager()->currentRenderData()->framing3D();
  } else if (mTrackballEnabled && !enabled) {
    // trackball gets switched off: check to reset framing
    if (QMessageBox::question(nullptr, "3D Framing", "Do you want to keep this 3D framing?") ==
        QMessageBox::StandardButton::No) {
      // reset framing
      mRenderer->renderObjectManager()->currentRenderData()->setFraming3D(
          mOriginalTrackballFraming);
    }
  }
  mTrackballEnabled = enabled;
}

void OpenGlWidget::initializeGL() {
  SPDLOG_INFO("Initializing OpenGlWidget");
  initializeOpenGLFunctions();
  // initialize the texture render object with all the OpenGL stuff
  mTextureRenderObject->initialize();
  auto outputResolution = QSize(1080, 720);
  mTextureRenderObject->changeTextureSizeAndFormat(outputResolution,
                                                   mTextureRenderObject->sourcePixelFormat());
  mTextureRenderObject->setFlipVertically(true);
  mTextureRenderObject->useExternalTexture(true);
  mTextureRenderObject->setFlipHorizontally(false);

  // update resolution and connect to settings change
  handleResolutionChange(outputResolution);
  winId();  // required to get correct pixel ratio for high dpi setups
  glEnable(GL_MULTISAMPLE);

  SPDLOG_WARN("Active viewport renderer: {}", this->glGetString(GL_RENDERER));

  
  tinygltf::Model model;
  tinygltf::TinyGLTF loader;
  std::string err;
  std::string warn;

  bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, "D:\\3dassets\\avocado\\Avocado.gltf");
  if (!warn.empty()) {
    SPDLOG_WARN("GLTF Warning: {}", warn);
  }
  if (!err.empty()) {
    SPDLOG_ERROR("GLTF Error: {}", err);
  }
  if (!ret) {
    SPDLOG_ERROR("Failed to load GLTF model");
    return;
  }

  // Process the model (e.g., create OpenGL buffers)
  processModel(model);

  /*
   */
}

void OpenGlWidget::paintGL() {
  // Bind default framebuffer
  QOpenGLFramebufferObject::bindDefault();
  glEnable(GL_MULTISAMPLE);

  // Setup output and background
  glViewport(mViewPort.x(), mViewPort.y(), mViewPort.width(), mViewPort.height());
  glClearColor(0.f, 0.f, 0.f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);  // Clear both color and depth buffers
  

  // Render RenderObjectManager's framebuffer as texture to screen
  // Only paint if there's a render object manager and it is initialized
  if (mRenderer && mRenderer->renderObjectManager() &&
      mRenderer->renderObjectManager()->isInitialized()) {
    glActiveTexture(GL_TEXTURE0 + mTextureRenderObject->colorTextureUnit());
    glBindTexture(
        TextureRenderObject::glTarget(mRenderer->renderObjectManager()->renderFrameBufferType()),
        mRenderer->renderObjectManager()->renderFrameBuffer()->texture());
  }

  // Render texture object without using its texture
  mTextureRenderObject->draw();
  glActiveTexture(GL_TEXTURE0);
  
  if (!mFirstDrawOccurred) {
    // Once the first time the buffer is drawn, emit the initialized signal
    emit initialized();
  }
  mFirstDrawOccurred = true;

  // Render the model
  renderModel();

  // Check for OpenGL errors (optional but recommended)
  GLenum err;
  while ((err = glGetError()) != GL_NO_ERROR) {
    SPDLOG_ERROR("OpenGL error: {}", err);
  }
}

void OpenGlWidget::resizeGL(int w, int h) {
  // needs pixel ratio for high dpi displays
  double pixelRatio = devicePixelRatioF();
  // Viewport: present framebuffer in a centered 16/9 format
  float ratio = static_cast<float>(width()) / height();
  if (ratio > (16.f / 9.f)) {
    // width bigger
    int newWidth = 16.f * height() / 9.f;
    mViewPort.setX(pixelRatio * (width() - newWidth) / 2);
    mViewPort.setY(0);
    mViewPort.setHeight(pixelRatio * height());
    mViewPort.setWidth(pixelRatio * newWidth);
  } else {
    // height bigger
    int newHeight = 9.f * width() / 16.f;
    mViewPort.setX(0);
    mViewPort.setY(pixelRatio * (height() - newHeight) / 2);
    mViewPort.setWidth(pixelRatio * width());
    mViewPort.setHeight(pixelRatio * newHeight);
  }
  QOpenGLWidget::resizeGL(w, h);
}

void OpenGlWidget::keyPressEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Shift) {
    mShiftKeyDown = true;
  }
  const float stepLength = 0.1f;
  if (!mRenderer) return;
  auto rom = mRenderer->renderObjectManager();
  if (!rom) return;
  const auto& renderData = rom->currentRenderData();
  if (mTrackballEnabled && renderData && renderData->is3D()) {
    RenderData::ShotFraming3D framing3D = rom->currentRenderData()->framing3D();
    QVector3D vector = framing3D.position();
    if (mShiftKeyDown) {
      vector = framing3D.lookAtPoint();
    }
    if (event->key() == Qt::Key_W) vector += QVector3D(0, 0, -stepLength);
    if (event->key() == Qt::Key_S) vector += QVector3D(0, 0, stepLength);
    if (event->key() == Qt::Key_A) vector += QVector3D(-stepLength, 0, 0);
    if (event->key() == Qt::Key_D) vector += QVector3D(stepLength, 0, 0);
    if (event->key() == Qt::Key_Q) vector += QVector3D(0, stepLength, 0);
    if (event->key() == Qt::Key_E) vector += QVector3D(0, -stepLength, 0);
    event->accept();
    if (mShiftKeyDown) {
      framing3D.setLookAtPoint(vector);
    } else {
      framing3D.setPosition(vector);
    }
    rom->currentRenderData()->setFraming3D(framing3D);
    updateRendering();
  }
}

void OpenGlWidget::keyReleaseEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Shift) {
    mShiftKeyDown = false;
  }
}

void OpenGlWidget::mousePressEvent(QMouseEvent* event) {
  if (!mRenderer) return;
  auto rom = mRenderer->renderObjectManager();
  if (!rom) return;

  const auto& renderData = rom->currentRenderData();
  event->ignore();
  if (mTrackballEnabled && renderData && renderData->is3D() && event->button() == Qt::LeftButton) {
    mLeftButtonDown = true;
    mLastMousePosition = event->globalPosition().toPoint();
  }
  if (!event->isAccepted()) {
    QOpenGLWidget::mousePressEvent(event);
  }
}

void OpenGlWidget::mouseReleaseEvent(QMouseEvent* event) {
  if (!mRenderer) return;
  auto rom = mRenderer->renderObjectManager();
  if (!rom) return;

  const auto& renderData = rom->currentRenderData();
  event->ignore();
  if (mTrackballEnabled && renderData && renderData->is3D() && event->button() == Qt::LeftButton) {
    mLeftButtonDown = false;
  }
  if (!event->isAccepted()) {
    QOpenGLWidget::mouseReleaseEvent(event);
  }
}

void OpenGlWidget::mouseMoveEvent(QMouseEvent* event) {
  if (!mRenderer) return;
  auto rom = mRenderer->renderObjectManager();
  if (!rom) return;

  const auto& renderData = rom->currentRenderData();
  event->ignore();
  if (mTrackballEnabled && mLeftButtonDown && renderData && renderData->is3D()) {
    // left button pressed (only in 3D)
    float factor = 0.035f;
    float differenceX = factor * (event->globalPosition().x() - mLastMousePosition.x());
    float differenceY = factor * (event->globalPosition().y() - mLastMousePosition.y());

    // Calculate rotation angles
    float angleX = differenceY;  // Rotate around X-axis based on Y movement
    float angleY = differenceX;  // Rotate around Y-axis based on X movement

    // Apply rotations to the transformation matrix
    mTransformMatrix.rotate(angleX, 5.0f, 0.0f, 0.0f);  // Rotate around X-axis
    mTransformMatrix.rotate(angleY, 0.0f, 5.0f, 0.0f);  // Rotate around Y-axis

    mLastMousePosition = event->globalPosition().toPoint();
    updateRendering();
    event->accept();
  }
  if (!event->isAccepted()) {
    QOpenGLWidget::mouseMoveEvent(event);
  }
}

void OpenGlWidget::wheelEvent(QWheelEvent* event) {
  event->ignore();
  if (!mRenderer) return;
  auto rom = mRenderer->renderObjectManager();
  if (!rom) return;

  const auto& renderData = rom->currentRenderData();
  if (mTrackballEnabled && renderData && renderData->is3D()) {
    QPointF delta = event->angleDelta();
    const float changeFactor = 1 / 20.f;
    RenderData::ShotFraming3D framing3D = renderData->framing3D();
    float angle = framing3D.fieldOfViewAngle();
    angle += delta.y() * changeFactor;
    const float minViewAngle = 5.f;
    const float maxViewAngle = 80.f;
    if (angle < minViewAngle) angle = minViewAngle;
    if (angle > maxViewAngle) angle = maxViewAngle;
    framing3D.setFieldOfViewAngle(angle);
    renderData->setFraming3D(framing3D);
    updateRendering();
    event->accept();
  }
  if (!event->isAccepted()) {
    QOpenGLWidget::wheelEvent(event);
  }
}

void OpenGlWidget::handleResolutionChange(QSize resolution) {
  makeCurrent();
  mTextureRenderObject->changeTextureSizeAndFormat(resolution,
                                                   mTextureRenderObject->sourcePixelFormat());
}

void OpenGlWidget::updateRendering() {
  update();
}

}  // namespace nimagna
