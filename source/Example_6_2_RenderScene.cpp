#include "Common.h"

struct PerEyeArg {
  Eye eye;
  glm::mat4 projectionOffset;
  glm::mat4 modelviewOffset;
  RiftLookupTexturePtr distortionTexture;

  PerEyeArg(Eye eye, const OVR::HMDInfo & ovrHmdInfo)
    : eye(eye) {
    OVR::Ptr<OVR::ProfileManager> profileManager = *OVR::ProfileManager::Create();
    OVR::Ptr<OVR::Profile> profile = *(profileManager->GetDeviceDefaultProfile(OVR::ProfileType::Profile_RiftDK1));
    float ipd = profile->GetIPD();
    modelviewOffset = glm::translate(glm::mat4(),
      glm::vec3(ipd / 2.0f, 0, 0));
    if (eye == RIGHT) {
      modelviewOffset = glm::inverse(modelviewOffset);
    }
    profileManager.Clear();

    OVR::Util::Render::StereoConfig config;
    config.SetHMDInfo(ovrHmdInfo);
    projectionOffset = glm::translate(glm::mat4(),
      glm::vec3(config.GetProjectionCenterOffset(), 0, 0));
    if (eye == RIGHT) {
      projectionOffset = glm::inverse(projectionOffset);
    }
  }
};

class SimpleScene: public RiftGlfwApp {
  glm::mat4 player;
  glm::mat4 projection;
  gl::TextureCubeMapPtr skyboxTexture;
  gl::GeometryPtr treeGeometry;
  float ipd;
  float eyeHeight;
  bool applyProjectionOffset;
  bool applyModelviewOffset;
  std::array<PerEyeArg, 2> eyes;

  gl::FrameBufferWrapper frameBuffer;
  gl::ProgramPtr distortProgram;
  gl::GeometryPtr quadGeometry;

public:
  SimpleScene() : eyes({ { PerEyeArg(LEFT, ovrHmdInfo), PerEyeArg(RIGHT, ovrHmdInfo) } }) {
    eyeHeight = 1.0f;
    applyProjectionOffset = true;
    applyModelviewOffset = true;
    OVR::Ptr<OVR::ProfileManager> profileManager = *OVR::ProfileManager::Create();
    OVR::Ptr<OVR::Profile> profile = *(profileManager->GetDeviceDefaultProfile(OVR::ProfileType::Profile_RiftDK1));
    ipd = profile->GetIPD();
    eyeHeight = profile->GetEyeHeight();

    OVR::Util::Render::StereoConfig config;
    config.SetHMDInfo(ovrHmdInfo);
    gl::Stacks::projection().top() =
      glm::perspective(config.GetYFOVRadians(), eyeAspect, 0.01f, 1000.0f);

    glm::vec3 playerPosition(0, eyeHeight, ipd * 4.0f);
    player = glm::inverse(glm::lookAt(playerPosition, glm::vec3(0, eyeHeight, 0), GlUtils::Y_AXIS));
    CameraControl::instance().enableHydra(true);
  }

  void initGl() {
    RiftGlfwApp::initGl();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    gl::clearColor(Colors::darkGrey);
    skyboxTexture = GlUtils::getCubemapTextures(Resource::IMAGES_SKY_CITY_XNEG_PNG);

    distortProgram = GlUtils::getProgram(
      Resource::SHADERS_TEXTURED_VS, 
      Resource::SHADERS_RIFTWARP_FS);
    distortProgram->use();
    distortProgram->setUniform1i("OffsetMap", 1);
    distortProgram->setUniform1i("Scene", 0);
    gl::Program::clear();

    quadGeometry = GlUtils::getQuadGeometry();
    frameBuffer.init(glm::uvec2(glm::vec2(eyeSize) * 2.0f));

    RiftDistortionHelper distortionHelper(ovrHmdInfo);
    FOR_EACH_EYE(eye) {
      eyes[eye].distortionTexture = 
        distortionHelper.createLookupTexture(glm::uvec2(512, 512), eye);
    }
  }

  virtual void onKey(int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) switch (key) {
    case GLFW_KEY_P:
      applyProjectionOffset = !applyProjectionOffset;
      return;
    case GLFW_KEY_M:
      applyModelviewOffset = !applyModelviewOffset;
      return;
    case GLFW_KEY_R:
      player = glm::inverse(glm::lookAt(glm::vec3(0, eyeHeight, ipd * 4.0f), glm::vec3(0, eyeHeight, 0), GlUtils::Y_AXIS));
      return;
    }

//    if (CameraControl::instance().onKey(player, key, scancode, action, mods)) {
//      return;
//    }

    RiftGlfwApp::onKey(key, scancode, action, mods);
  }

  virtual void update() {
    CameraControl::instance().applyInteraction(player);
    gl::Stacks::modelview().top() = glm::inverse(player);
  }

  void draw() {
    glClear(GL_COLOR_BUFFER_BIT);
    FOR_EACH_EYE(eye) {
      const PerEyeArg & eyeArgs = eyes[eye];
      frameBuffer.activate();
      glEnable(GL_DEPTH_TEST);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
      renderScene(eyeArgs);
      frameBuffer.deactivate();
      glDisable(GL_DEPTH_TEST);

      viewport(eye);
      distortProgram->use();
      glActiveTexture(GL_TEXTURE1);
      eyeArgs.distortionTexture->bind();
      glActiveTexture(GL_TEXTURE0);
      frameBuffer.color->bind();
      quadGeometry->bindVertexArray();
      quadGeometry->draw();
      gl::VertexArray::unbind();
      gl::Program::clear();
    }
  }

  void renderScene(const PerEyeArg & eyeArg) {
    gl::MatrixStack & pr = gl::Stacks::projection();
    gl::MatrixStack & mv = gl::Stacks::modelview();
    pr.push();
    mv.push();
    if (applyProjectionOffset) {
      pr.preMultiply(eyeArg.projectionOffset);
    }
    if (applyModelviewOffset) {
      mv.preMultiply(eyeArg.modelviewOffset);
    }

    GlUtils::renderSkybox(Resource::IMAGES_SKY_CITY_XNEG_PNG);
    GlUtils::renderFloorGrid(player);
    mv.push().translate(glm::vec3(0, eyeHeight, 0)).scale(ipd);
    GlUtils::drawColorCube();
    mv.pop();

    mv.pop();
    pr.pop();
  }
};

RUN_OVR_APP(SimpleScene);