#include "Rendering/PlyRenderObject.h"

#include <QDebug>
#include <QFile>
#include <QtOpenGL/QOpenGLShader>
#include <filesystem>
#include <iostream>

namespace nimagna {

PlyRenderObject::PlyRenderObject(const QString& location) : mGsLocation(location) {
  initialize();
}

PlyRenderObject::~PlyRenderObject() {
  // QOpenGLShaderProgram will be deleted automatically by unique_ptr
}
void PlyRenderObject::BuildVertexArrayObject(std::shared_ptr<GaussianCloud> gaussianCloud) {
  splatVao = std::make_shared<VertexArrayObject>();

  // allocate large buffer to hold interleaved vertex data
  gaussianDataBuffer = std::make_shared<BufferObject>(
      GL_ARRAY_BUFFER, gaussianCloud->GetRawDataPtr(), gaussianCloud->GetTotalSize(), 0);

  const size_t numGaussians = gaussianCloud->GetNumGaussians();

  // build element array
  indexVec.reserve(numGaussians);
  assert(numGaussians <= std::numeric_limits<uint32_t>::max());
  for (uint32_t i = 0; i < (uint32_t)numGaussians; i++) {
    indexVec.push_back(i);
  }
  auto indexBuffer =
      std::make_shared<BufferObject>(GL_ELEMENT_ARRAY_BUFFER, indexVec, GL_DYNAMIC_STORAGE_BIT);

  splatVao->Bind();
  gaussianDataBuffer->Bind();

  const size_t stride = gaussianCloud->GetStride();
  SetupAttrib(splatProg->GetAttribLoc("position"), gaussianCloud->GetPosWithAlphaAttrib(), 4,
              stride);
  SetupAttrib(splatProg->GetAttribLoc("r_sh0"), gaussianCloud->GetR_SH0Attrib(), 4, stride);
  SetupAttrib(splatProg->GetAttribLoc("g_sh0"), gaussianCloud->GetG_SH0Attrib(), 4, stride);
  SetupAttrib(splatProg->GetAttribLoc("b_sh0"), gaussianCloud->GetB_SH0Attrib(), 4, stride);
  if (gaussianCloud->HasFullSH()) {
    SetupAttrib(splatProg->GetAttribLoc("r_sh1"), gaussianCloud->GetR_SH1Attrib(), 4, stride);
    SetupAttrib(splatProg->GetAttribLoc("r_sh2"), gaussianCloud->GetR_SH2Attrib(), 4, stride);
    SetupAttrib(splatProg->GetAttribLoc("r_sh3"), gaussianCloud->GetR_SH3Attrib(), 4, stride);
    SetupAttrib(splatProg->GetAttribLoc("g_sh1"), gaussianCloud->GetG_SH1Attrib(), 4, stride);
    SetupAttrib(splatProg->GetAttribLoc("g_sh2"), gaussianCloud->GetG_SH2Attrib(), 4, stride);
    SetupAttrib(splatProg->GetAttribLoc("g_sh3"), gaussianCloud->GetG_SH3Attrib(), 4, stride);
    SetupAttrib(splatProg->GetAttribLoc("b_sh1"), gaussianCloud->GetB_SH1Attrib(), 4, stride);
    SetupAttrib(splatProg->GetAttribLoc("b_sh2"), gaussianCloud->GetB_SH2Attrib(), 4, stride);
    SetupAttrib(splatProg->GetAttribLoc("b_sh3"), gaussianCloud->GetB_SH3Attrib(), 4, stride);
  }
  SetupAttrib(splatProg->GetAttribLoc("cov3_col0"), gaussianCloud->GetCov3_Col0Attrib(), 3, stride);
  SetupAttrib(splatProg->GetAttribLoc("cov3_col1"), gaussianCloud->GetCov3_Col1Attrib(), 3, stride);
  SetupAttrib(splatProg->GetAttribLoc("cov3_col2"), gaussianCloud->GetCov3_Col2Attrib(), 3, stride);

  splatVao->SetElementBuffer(indexBuffer);
  gaussianDataBuffer->Unbind();
}

void PlyRenderObject::initialize() {
  initializeOpenGLFunctions();
  // setupShaderProgram();
  //  Additional initialization (buffers, VAO, textures) can be added here

//  glDisable(GL_FRAMEBUFFER_SRGB);
  bool isFramebufferSRGBEnabled = false;

  // gaussianCloud = LoadGaussianCloud(mGsLocation, opt);

  GaussianCloud::Options options = {0};

  options.importFullSH = true;
  options.exportFullSH = true;
  gaussianCloud = std::make_shared<GaussianCloud>(options);
  if (!gaussianCloud->ImportPly(mGsLocation.toStdString())) {
    SPDLOG_ERROR("Error loading GaussianCloud!\n");
    return;
  }
 // splatRenderer = std::make_shared<SplatRenderer>();

  bool useRgcSortOverride = false;
 /* if (!splatRenderer->Init(gaussianCloud, isFramebufferSRGBEnabled, useRgcSortOverride)) {
    SPDLOG_ERROR("Error initializing splat renderer!\n");
    return;
  }*/
   
  GL_ERROR_CHECK("SplatRenderer::Init() begin"); 
  splatProg = std::make_shared<Program>(); 
  if (isFramebufferSRGBEnabled || gaussianCloud->HasFullSH()) {
    std::string defines = "";
    if (isFramebufferSRGBEnabled) {
      defines += "#define FRAMEBUFFER_SRGB\n";
    }
    if (gaussianCloud->HasFullSH()) {
      defines += "#define FULL_SH\n";
    }
    splatProg->AddMacro("DEFINES", defines);
  }
  if (!splatProg->LoadVertGeomFrag("MainApplication/resources/shaders/splat_vert.glsl", "MainApplication/resources/shaders/splat_geom.glsl",
                                   "MainApplication/resources/shaders/splat_frag.glsl")) {
    SPDLOG_ERROR("Error loading splat shaders!\n"); 
  }

  preSortProg = std::make_shared<Program>();
  if (!preSortProg->LoadCompute("MainApplication/resources/shaders/presort_compute.glsl")) {
    SPDLOG_ERROR("Error loading pre-sort compute shader!\n"); 
  } 
  bool useMultiRadixSort = !useRgcSortOverride;
  if (useMultiRadixSort) {
    sortProg = std::make_shared<Program>();
    if (!sortProg->LoadCompute("MainApplication/resources/shaders/multi_radixsort.glsl")) {
      SPDLOG_ERROR("Error loading sort compute shader!\n"); 
    }

    histogramProg = std::make_shared<Program>();
    if (!histogramProg->LoadCompute("MainApplication/resources/shaders/multi_radixsort_histograms.glsl")) {
      SPDLOG_ERROR("Error loading histogram compute shader!\n"); 
    }
  }
   

  // build posVec
  size_t numGaussians = gaussianCloud->GetNumGaussians();
  posVec.reserve(numGaussians);
  gaussianCloud->ForEachPosWithAlpha(
      [this](const float* pos) { posVec.emplace_back(glm::vec4(pos[0], pos[1], pos[2], 1.0f)); });

  BuildVertexArrayObject(gaussianCloud);

  depthVec.resize(numGaussians);

  if (useMultiRadixSort) {
    printf("using multi_radixsort.glsl\n");

    keyBuffer =
        std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, depthVec, GL_DYNAMIC_STORAGE_BIT);
    keyBuffer2 =
        std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, depthVec, GL_DYNAMIC_STORAGE_BIT);

    const uint32_t NUM_ELEMENTS = static_cast<uint32_t>(numGaussians);
    const uint32_t NUM_WORKGROUPS =
        (NUM_ELEMENTS + numBlocksPerWorkgroup - 1) / numBlocksPerWorkgroup;
    const uint32_t RADIX_SORT_BINS = 256;

    std::vector<uint32_t> histogramVec(NUM_WORKGROUPS * RADIX_SORT_BINS, 0);
    histogramBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, histogramVec,
                                                     GL_DYNAMIC_STORAGE_BIT);

    valBuffer =
        std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, indexVec, GL_DYNAMIC_STORAGE_BIT);
    valBuffer2 =
        std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, indexVec, GL_DYNAMIC_STORAGE_BIT);
    posBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, posVec);
  } else {
    printf("using rgc::radix_sort\n");
    keyBuffer =
        std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, depthVec, GL_DYNAMIC_STORAGE_BIT);
    valBuffer =
        std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, indexVec, GL_DYNAMIC_STORAGE_BIT);
    posBuffer = std::make_shared<BufferObject>(GL_SHADER_STORAGE_BUFFER, posVec);

    sorter = std::make_shared<rgc::radix_sort::sorter>(numGaussians);
  }

  atomicCounterVec.resize(1, 0);
  atomicCounterBuffer = std::make_shared<BufferObject>(GL_ATOMIC_COUNTER_BUFFER, atomicCounterVec,
                                                       GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT);

  GL_ERROR_CHECK("SplatRenderer::Init() end");

  /*  desktopProgram = std::make_unique<QOpenGLShaderProgram>();
  if (!desktopProgram->addCacheableShaderFromSourceFile(QOpenGLShader::Vertex,
                                                        ":/resources/shaders/desktop_vert.glsl")) {
    SPDLOG_ERROR("Error loading Vertex shader!\n");
    return;
  }
  if (!desktopProgram->addCacheableShaderFromSourceFile(QOpenGLShader::Fragment,
                                                        ":/resources/shaders/desktop_frag.glsl")) {
    SPDLOG_ERROR("Error loading Fragment shader!\n");
    return;
  }*/
}
void PlyRenderObject::Sort(const QMatrix4x4& cameraMat, const QMatrix4x4& projMat,
                         const QVector4D& viewport, const QVector2D& nearFar) {
 
  GL_ERROR_CHECK("SplatRenderer::Sort() begin");

  const size_t numPoints = posVec.size();
  QMatrix4x4 modelViewMat = cameraMat.inverted(); 

  bool useMultiRadixSort =true;

  // 24 bit radix sort still has some artifacts on some datasets, so use 32 bit sort.
  // const uint32_t NUM_BYTES = useMultiRadixSort ? 3 : 4;
  // const uint32_t MAX_DEPTH = useMultiRadixSort ? 16777215 : std::numeric_limits<uint32_t>::max();
  const uint32_t NUM_BYTES = 4;
  const uint32_t MAX_DEPTH = std::numeric_limits<uint32_t>::max();

  { 

    preSortProg->Bind();
    preSortProg->SetUniform("modelViewProj", projMat * modelViewMat);
    preSortProg->SetUniform("nearFar", nearFar);
    preSortProg->SetUniform("keyMax", MAX_DEPTH);

    // reset counter back to zero
    atomicCounterVec[0] = 0;
    atomicCounterBuffer->Update(atomicCounterVec);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, posBuffer->GetObj());  // readonly
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, keyBuffer->GetObj());  // writeonly
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, valBuffer->GetObj());  // writeonly
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 4, atomicCounterBuffer->GetObj());

    const int LOCAL_SIZE = 256;
    glDispatchCompute(((GLuint)numPoints + (LOCAL_SIZE - 1)) / LOCAL_SIZE, 1,
                      1);  // Assuming LOCAL_SIZE threads per group
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);

    GL_ERROR_CHECK("SplatRenderer::Sort() pre-sort");
  }

  { 

    atomicCounterBuffer->Read(atomicCounterVec);
    sortCount = atomicCounterVec[0];

    assert(sortCount <= (uint32_t)numPoints);

    GL_ERROR_CHECK("SplatRenderer::Render() get-count");
  }

  if (useMultiRadixSort) { 

    const uint32_t NUM_ELEMENTS = static_cast<uint32_t>(sortCount);
    const uint32_t NUM_WORKGROUPS =
        (NUM_ELEMENTS + numBlocksPerWorkgroup - 1) / numBlocksPerWorkgroup;

    sortProg->Bind();
    sortProg->SetUniform("g_num_elements", NUM_ELEMENTS);
    sortProg->SetUniform("g_num_workgroups", NUM_WORKGROUPS);
    sortProg->SetUniform("g_num_blocks_per_workgroup", numBlocksPerWorkgroup);

    histogramProg->Bind();
    histogramProg->SetUniform("g_num_elements", NUM_ELEMENTS);
    // histogramProg->SetUniform("g_num_workgroups", NUM_WORKGROUPS);
    histogramProg->SetUniform("g_num_blocks_per_workgroup", numBlocksPerWorkgroup);

    for (uint32_t i = 0; i < NUM_BYTES; i++) {
      histogramProg->Bind();
      histogramProg->SetUniform("g_shift", 8 * i);

      if (i == 0 || i == 2) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, keyBuffer->GetObj());
      } else {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, keyBuffer2->GetObj());
      }
      glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, histogramBuffer->GetObj());

      glDispatchCompute(NUM_WORKGROUPS, 1, 1);

      glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

      sortProg->Bind();
      sortProg->SetUniform("g_shift", 8 * i);

      if ((i % 2) == 0)  // even
      {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, keyBuffer->GetObj());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, keyBuffer2->GetObj());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, valBuffer->GetObj());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, valBuffer2->GetObj());
      } else  // odd
      {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, keyBuffer2->GetObj());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, keyBuffer->GetObj());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, valBuffer2->GetObj());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, valBuffer->GetObj());
      }
      glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, histogramBuffer->GetObj());

      glDispatchCompute(NUM_WORKGROUPS, 1, 1);

      glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    GL_ERROR_CHECK("SplatRenderer::Sort() sort");

    // indicate if keys are sorted properly or not.
    if (false) {
      std::vector<uint32_t> sortedKeyVec(numPoints, 0);
      keyBuffer->Read(sortedKeyVec);

      GL_ERROR_CHECK("SplatRenderer::Sort() READ buffer");

      bool sorted = true;
      for (uint32_t i = 1; i < sortCount; i++) {
        if (sortedKeyVec[i - 1] > sortedKeyVec[i]) {
          sorted = false;
          break;
        }
      }

      printf("%s", sorted ? "o" : "x");
    }
  } else { 
    sorter->sort(keyBuffer->GetObj(), valBuffer->GetObj(), sortCount);
    GL_ERROR_CHECK("SplatRenderer::Sort() rgc sort");
  }

  { 

    if (useMultiRadixSort && (NUM_BYTES % 2) == 1)  // odd
    {
      glBindBuffer(GL_COPY_READ_BUFFER, valBuffer2->GetObj());
    } else {
      glBindBuffer(GL_COPY_READ_BUFFER, valBuffer->GetObj());
    }
    glBindBuffer(GL_COPY_WRITE_BUFFER, splatVao->GetElementBuffer()->GetObj());
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0,
                        sortCount * sizeof(uint32_t));

    GL_ERROR_CHECK("SplatRenderer::Sort() copy-sorted");
  }
}

void PlyRenderObject::draw(const QMatrix4x4& viewMatrix, const QMatrix4x4& projectionMatrix) {
  
  Clear();
  QSize viewportSize = QOpenGLContext::currentContext()->screen()->size();
 
  QVector4D viewport(0.0f, 0.0f, (float)viewportSize.width(), (float)viewportSize.height());
  QVector2D nearFar(znear, zfar);
  gsprojectionMatrix.perspective(fovY(), (float)viewportSize.width() / (float)viewportSize.height(),  znear, zfar);
  // Convert gsprojectionMatrix (assumed QMatrix4x4) to glm::mat4 
 /* if ( pointRenderer) {
    pointRenderer->Render(cameraMat, projMat, viewport, nearFar);
  } else {*/
        Sort(viewMatrix, gsprojectionMatrix, viewport, nearFar);
  GL_ERROR_CHECK("SplatRenderer::Render() begin"); 
  { 
    float width = viewport.z();
    float height = viewport.w();
    float aspectRatio = width / height; 
    glm::vec3 eye = glm::vec3(viewMatrix(0, 3), viewMatrix(1, 3), viewMatrix(2, 3));

    splatProg->Bind();
    splatProg->SetUniform("viewMat", viewMatrix);
    splatProg->SetUniform("projMat", gsprojectionMatrix);
    splatProg->SetUniform("viewport", viewport);
    splatProg->SetUniform("projParams", glm::vec4(0.0f, nearFar.x(), nearFar.y(), 0.0f));
    splatProg->SetUniform("eye", eye);

    splatVao->Bind();
    glDrawElements(GL_POINTS, sortCount, GL_UNSIGNED_INT, nullptr);
    splatVao->Unbind();
    GL_ERROR_CHECK("SplatRenderer::Render() draw");
  }
  //} 
}

void PlyRenderObject::setupShaderProgram() {}
// searches for file named configFilename, dir that contains plyFilename, it's parent and
// grandparent dirs.
QString PlyRenderObject::FindConfigFile(const QString& plyFilename, const QString& configFilename) {
  std::filesystem::path plyPath(plyFilename.toStdString());
  if (!std::filesystem::exists(plyPath) || !std::filesystem::is_regular_file(plyPath)) {
    qWarning() << "PLY file does not exist or is not a file: " << plyFilename.toStdString().c_str();
    return "";
  }
  std::filesystem::path directory = plyPath.parent_path();
  for (int i = 0; i < 3; ++i)  // Check current, parent, and grandparent directories
  {
    std::filesystem::path configPath = directory / configFilename.toStdString();
    if (std::filesystem::exists(configPath) && std::filesystem::is_regular_file(configPath)) {
      return QString::fromStdString(configPath.string());
    }
    if (directory.has_parent_path()) {
      directory = directory.parent_path();
    } else {
      break;
    }
  }
  return "";
}

QString PlyRenderObject::GetFilenameWithoutExtension(const QString& filepath) {
  std::filesystem::path pathObj(filepath.toStdString());
  // Check if the path has a stem (the part of the path before the extension)
  if (pathObj.has_stem()) {
    return QString::fromStdString(pathObj.stem().string());
  }
  // If there is no stem, return an empty string
  return "";
}

void PlyRenderObject::Clear() {
  //// pre-multiplied alpha blending
  // glEnable(GL_BLEND);
  ////glBlendEquation(GL_FUNC_ADD);
  // glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  // glm::vec4 clearColor(0.0f, 0.0f, 0.0f, 1.0f);
  // glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
  // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  //// NOTE: if depth buffer has less then 24 bits, it can mess up splat rendering.
  // glEnable(GL_DEPTH_TEST);
}

}  // namespace nimagna
