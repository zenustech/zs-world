/*
 * Basic camera class
 *
 * Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include <glm/ext/matrix_transform.hpp>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "world/WorldExport.hpp"

#if defined(_WIN32)
#  undef far
#  undef near
#endif

class ZS_WORLD_EXPORT Camera {
  float fov, aspect;
  float znear, zfar;

public:
  glm::vec3 getCameraFront() {
    glm::vec3 camFront;
    camFront.x = -cos(glm::radians(rotation.x)) * sin(glm::radians(rotation.y));
    camFront.y = sin(glm::radians(rotation.x));
    camFront.z = cos(glm::radians(rotation.x)) * cos(glm::radians(rotation.y));
    return glm::normalize(camFront);
  }

  glm::vec3 getCameraRayDirection(float w, float h, float width, float height) const {
    // assert(width > height);
    auto camToWorld = glm::inverse(matrices.view);
    auto scale = glm::tan(glm::radians(fov * 0.5f));
    float x = (2 * w / width - 1) * aspect * scale;
    float y = (1 - 2 * h / height) * scale;
    auto dir4 = camToWorld * glm::vec4(x, y, -1, 0);
    return glm::normalize(glm::vec3(dir4));
  }

  glm::vec2 getScreenPoint(const glm::vec3& worldPos, float width, float height) const {
    auto ndc = matrices.perspective * matrices.view * glm::vec4(worldPos, 1.f);
    ndc.x /= ndc.w;
    ndc.y /= ndc.w;
    return glm::vec2((1 + ndc.x) * 0.5f * width, height - (1 + ndc.y) * 0.5f * height);
  }

  void updateViewMatrix() {
    glm::mat4 rotM = glm::mat4(1.0f);
    glm::mat4 transM;

    rotM = glm::rotate(rotM, glm::radians(rotation.x * (flipY ? -1.0f : 1.0f)),
                       glm::vec3(1.0f, 0.0f, 0.0f));
    rotM = glm::rotate(rotM, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    rotM = glm::rotate(rotM, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

    glm::vec3 translation = position;
    if (flipY) {
      translation.y *= -1.0f;
    }
    transM = glm::translate(glm::mat4(1.0f), translation);

    if (type == CameraType::firstperson) {
      matrices.view = rotM * transM;
    } else {
      matrices.view = transM * rotM;
    }

    viewPos = glm::vec4(position, 0.0f) * glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f);
    updatePlanes();

    updated = true;
  };

  enum CameraType { lookat, firstperson };
  CameraType type = CameraType::lookat;

  glm::vec3 rotation = glm::vec3();
  glm::vec3 position = glm::vec3();
  glm::vec4 viewPos = glm::vec4();

  float rotationSpeed = 1.0f;
  float movementSpeed = 1.0f;

  bool updated = false;
  bool flipY = false;
  bool reversedZ = false;

  // variables for frustum culling
  glm::vec3 planeNormals[5];  // plane normals for left, right, top, bottom, near, EXCLUDING far
  float planeOffsets[5];      // plane offsets for left, right, top, bottom, near, EXCLUDING far
  glm::vec2 invCosFOV;
  glm::vec2 tanFOV = glm::vec2();

  struct {
    glm::mat4 perspective;
    glm::mat4 view;
  } matrices;

  struct {
    bool left = false;
    bool right = false;
    bool up = false;
    bool down = false;
  } keys;

  bool moving() { return keys.left || keys.right || keys.up || keys.down; }

  bool isUpdated(bool clear = false) {
    auto ret = updated;
    if (clear) updated = false;
    return ret;
  }
  float getFov() const { return fov; }
  float getAspect() const { return aspect; }
  float getNearClip() const { return znear; }
  float getFarClip() const { return zfar; }
  bool getFlipY() const { return flipY; }
  bool getReversedZ() const { return reversedZ; }
  glm::vec3 getPosition() const { return position; }
  glm::vec3 getRotation() const { return rotation; }

  void setFlipY(bool flipY) {
    this->flipY = flipY;
    updated = true;
  }
  void setReversedZ(bool reversedZ) {
    this->reversedZ = reversedZ;
    updated = true;
  }
  void setPerspective(float fov, float aspect, float znear, float zfar) {
    this->fov = fov;
    this->aspect = aspect;
    this->znear = znear;
    this->zfar = zfar;
    if (this->reversedZ) {
      matrices.perspective = glm::perspectiveZO(glm::radians(fov), aspect, zfar, znear);
    } else {
      matrices.perspective = glm::perspectiveZO(glm::radians(fov), aspect, znear, zfar);
    }
    if (flipY) {
      matrices.perspective[1][1] *= -1.0f;
    }

    this->tanFOV.y = glm::tan(glm::radians(fov * 0.5f));
    this->tanFOV.x = this->tanFOV.y * aspect;
    // float x = znear * this->tanFOV.x; // half width of near plane
    // this->invCosFOV.x = 1.0f / (znear / sqrt(x * x + znear * znear));
    // this->invCosFOV.y = 1.0f / cos(glm::radians(fov * 0.5f));

    updated = true;
  };

  void updateAspectRatio(float aspect) {
    this->aspect = aspect;
    matrices.perspective = glm::perspective(glm::radians(fov), aspect, znear, zfar);
    if (flipY) {
      matrices.perspective[1][1] *= -1.0f;
    }
    updated = true;
  }

  void setPosition(glm::vec3 position) {
    this->position = position;
    updateViewMatrix();
  }

  void setRotation(glm::vec3 rotation) {
    this->rotation = rotation;
    updateViewMatrix();
  }

  void rotate(glm::vec3 delta) {
    this->rotation += delta;
    updateViewMatrix();
  }

  void setTranslation(glm::vec3 translation) {
    this->position = translation;
    updateViewMatrix();
  };

  glm::vec3 _roughUp() { return glm::vec3(0, cos(glm::radians(rotation.x)) > 0 ? 1 : -1, 0); }
  void translate(glm::vec3 delta) {
    if (type == CameraType::firstperson) {
      glm::vec3 camFront = getCameraFront();
      auto right = glm::normalize(glm::cross(camFront, _roughUp()));
      auto up = glm::normalize(glm::cross(camFront, right));

      position += delta.x * right + delta.y * up;
    }
    updateViewMatrix();
  }
  void translateHorizontal(float delta) {
    if (type == CameraType::firstperson) {
      glm::vec3 camFront = getCameraFront();

      /// @note: delta positive towards right; otherwise left
      position += glm::normalize(glm::cross(camFront, _roughUp())) * delta;
    }
    updated = true;
  }
  void translateForward(float delta) {
    if (type == CameraType::firstperson) {
      glm::vec3 camFront = getCameraFront();

      /// @note: delta positive towards forward; otherwise backward
      position += camFront * delta;
    }
    updated = true;
  }
  void translateVertical(float delta) {
    if (type == CameraType::firstperson) {
      glm::vec3 camFront = getCameraFront();

      auto up = glm::normalize(glm::cross(camFront, glm::cross(camFront, _roughUp())));

      position += up * delta;
    }
    updated = true;
  }
  void yaw(float delta) {  // y-axis, rotate around x-axis
    if (cos(glm::radians(rotation.x)) > 0)
      rotation.y += delta;
    else
      rotation.y -= delta;
    updated = true;
  }
  void pitch(float delta) {  // x-axis, rotate around y-axis
    rotation.x -= delta;
    updated = true;
  }

  void setRotationSpeed(float rotationSpeed) {
    this->rotationSpeed = rotationSpeed;
    updated = true;
  }

  void setMovementSpeed(float movementSpeed) {
    this->movementSpeed = movementSpeed;
    updated = true;
  }

  // frustum culling
  void updatePlanes();
  bool isSphereVisible(const glm::vec3& worldSpaceCenter, float radius) const;
  bool isAABBVisible(const glm::vec3& a, const glm::vec3& b) const;

  void update(float deltaTime) {
    updated = false;
    if (type == CameraType::firstperson) {
      if (moving()) {
        glm::vec3 camFront = getCameraFront();

        float moveSpeed = deltaTime * movementSpeed;

        if (keys.up) position += camFront * moveSpeed;
        if (keys.down) position -= camFront * moveSpeed;
        if (keys.left) position -= glm::normalize(glm::cross(camFront, _roughUp())) * moveSpeed;
        if (keys.right) position += glm::normalize(glm::cross(camFront, _roughUp())) * moveSpeed;
      }
    }
    updateViewMatrix();
  };

  // Update camera passing separate axis data (gamepad)
  // Returns true if view or position has been changed
  bool updatePad(glm::vec2 axisLeft, glm::vec2 axisRight, float deltaTime) {
    bool retVal = false;

    if (type == CameraType::firstperson) {
      // Use the common console thumbstick layout
      // Left = view, right = move

      const float deadZone = 0.0015f;
      const float range = 1.0f - deadZone;

      glm::vec3 camFront = getCameraFront();

      float moveSpeed = deltaTime * movementSpeed * 2.0f;
      float rotSpeed = deltaTime * rotationSpeed * 50.0f;

      // Move
      if (fabsf(axisLeft.y) > deadZone) {
        float pos = (fabsf(axisLeft.y) - deadZone) / range;
        position -= camFront * pos * ((axisLeft.y < 0.0f) ? -1.0f : 1.0f) * moveSpeed;
        retVal = true;
      }
      if (fabsf(axisLeft.x) > deadZone) {
        float pos = (fabsf(axisLeft.x) - deadZone) / range;
        position += glm::normalize(glm::cross(camFront, _roughUp())) * pos
                    * ((axisLeft.x < 0.0f) ? -1.0f : 1.0f) * moveSpeed;
        retVal = true;
      }

      // Rotate
      if (fabsf(axisRight.x) > deadZone) {
        float pos = (fabsf(axisRight.x) - deadZone) / range;
        rotation.y += pos * ((axisRight.x < 0.0f) ? -1.0f : 1.0f) * rotSpeed;
        retVal = true;
      }
      if (fabsf(axisRight.y) > deadZone) {
        float pos = (fabsf(axisRight.y) - deadZone) / range;
        rotation.x -= pos * ((axisRight.y < 0.0f) ? -1.0f : 1.0f) * rotSpeed;
        retVal = true;
      }
    } else {
      // todo: move code from example base class for look-at
    }

    if (retVal) {
      updateViewMatrix();
    }

    return retVal;
  }
};
/*
All physical unit below, are in International System of Units(SI).
All angle unit below are in rad.
*/

struct CameraRenderInfo {
  float focal_length = 0.055;
  float near = 0.001;
  float far = 1000;
  glm::vec2 sensor_size;  // x for horizontal, y for vertical same blow
  glm::vec2 sensor_offset;
};

struct CameraGeoInfo {
  glm::vec3 eye;
  glm::vec3 up;
  glm::vec3 right;
  glm::vec3 view;
  glm::vec3 lock_pos;
};
class CameraBase {
public:
  // Unless you know what you are doing, Otherwise:
  // DO NOT edit these two structs directly, use get/set method.
  struct CameraGeoInfo geo;
  struct CameraRenderInfo render_info;

  CameraBase() {
    geo.eye = glm::vec3(0, 0, 0);
    geo.up = glm::vec3(0, 1, 0);
    geo.right = glm::vec3(1, 0, 0);
    geo.view = glm::vec3(0, 0, -1);
    geo.lock_pos = glm::vec3(0, 0, -1);
    render_info.sensor_size = glm::vec2(0.036, 0.024);
    render_info.sensor_offset = glm::vec2(0);
  }

  bool isRolllingEnable() { return rolling_enable; }
  void setRollingEnable() { rolling_enable = true; }
  void setRollingDisable() { rolling_enable = false; }

  bool isLocked() { return !free_camera; }

  void setFree() { free_camera = true; }

  void forceRollBack(bool force_positive_Y_up = false) {
    float flag = geo.up.y;
    if (flag != 0) {
      if (force_positive_Y_up) {
        flag = 1;
      }
      glm::vec3 tmp_up = glm::vec3(0, flag, 0);
      tmp_up = glm::normalize(tmp_up);

      glm::vec3 tmp_vec = glm::vec3(geo.view.x, 0, geo.view.z);

      geo.right = glm::cross(tmp_vec, tmp_up);
      geo.right = glm::normalize(geo.right);

      geo.up = glm::cross(geo.right, geo.view);
    }
  }

  void autoRollBack() {
    if (!isRolllingEnable()) {
      forceRollBack();
    }
  }

  void setLockTo(glm::vec3 lock_position) {
    free_camera = false;
    geo.lock_pos = lock_position;
    geo.view = glm::normalize(geo.lock_pos - geo.eye);
    forceRollBack(true);
  }

  void moveCamera(glm::vec3 direction) {
    glm::vec3 diff_right = geo.right * direction.x;
    glm::vec3 diff_up = geo.up * direction.y;
    glm::vec3 diff_view = geo.view * direction.z;
    glm::vec3 diff_vec = diff_right + diff_up + diff_view;

    geo.eye += diff_vec;

    geo.lock_pos += diff_right;
    geo.lock_pos += diff_up;
  }

  void rotateCamera(glm::vec2 direction) {
    glm::mat4 translate = glm::mat4(1);

    if (isRolllingEnable()) {
      glm::vec3 tanDirection = geo.right * direction.x + geo.up * direction.y;
      tanDirection = glm::normalize(tanDirection);
      glm::vec3 axis = glm::cross(tanDirection, geo.view);
      translate = glm::rotate(translate, glm::length(direction), axis);
    } else {
      if (geo.up.y != 0) {
        glm::vec3 axis = glm::vec3(0, geo.up.y, 0);
        axis = glm::normalize(axis);
        translate = glm::rotate(translate, direction.x, axis);
      }
      translate = glm::rotate(translate, direction.y, geo.right);
    }
    geo.view = translate * glm::vec4(geo.view, 1);
    geo.right = translate * glm::vec4(geo.right, 1);
    geo.up = translate * glm::vec4(geo.up, 1);

    if (isLocked()) {
      geo.eye -= geo.lock_pos;
      geo.eye = translate * glm::vec4(geo.eye, 1);
      geo.eye += geo.lock_pos;
    }
  }

  glm::vec3 getPosition() { return geo.eye; }
  glm::vec3 getView() { return geo.view; }
  glm::vec3 getFront() { return geo.view; }
  glm::vec3 getUp() { return geo.up; }
  glm::vec3 getRight() { return geo.right; }
  glm::mat4 getViewMatrix() {
    glm::mat4 view_matrix;
    if (isLocked()) {
      view_matrix = glm::lookAtRH(geo.eye, geo.lock_pos, geo.up);
    } else {
      view_matrix = glm::lookAtRH(geo.eye, geo.eye + geo.view, geo.up);
    }
    return view_matrix;
  }
  glm::vec2 getSensorSize() { return render_info.sensor_size; }
  float getFocalLength() { return render_info.focal_length; }
  glm::vec2 getSensorOffset() { return render_info.sensor_offset; }
  void setSensorSize(glm::vec2 new_size) { render_info.sensor_size = new_size; }
  void setFocalLength(float focal_length) { render_info.focal_length = focal_length; }
  void setSensorOffset(glm::vec2 offset) { render_info.sensor_offset = offset; }
  void setPosition(glm::vec3 new_eye) {
    geo.eye = new_eye;
    if (isLocked()) {
      geo.view = geo.lock_pos - geo.eye;
      forceRollBack();
    }
  }
  void rollCamera(float roll) {
    if (isRolllingEnable()) {
      glm::mat4 translate = glm::mat4(1);
      translate = glm::rotate(translate, roll, geo.view);
      geo.up = translate * glm::vec4(geo.up, 1);
      geo.right = translate * glm::vec4(geo.right, 1);
    }
  }
  void rollCameraAbs(float roll) {
    if (isRolllingEnable()) {
      forceRollBack(true);
      glm::mat4 translate = glm::mat4(1);
      translate = glm::rotate(translate, roll, geo.view);
      geo.up = translate * glm::vec4(geo.up, 1);
      geo.right = translate * glm::vec4(geo.right, 1);
    }
  }

private:
  bool rolling_enable = false;
  bool free_camera = true;
};

enum AOV_TYPE { HORIZONTAL, VERTICAL, DIAGONAL };
class RasterizationCamera : public CameraBase {
public:
  RasterizationCamera() : CameraBase() {}
  float getAspect() { return render_info.sensor_size.x / render_info.sensor_size.y; }
  float getAov(AOV_TYPE type) {
    float L = 0;
    switch (type) {
      case HORIZONTAL:
        L = render_info.sensor_size.x;
      case VERTICAL:
        L = render_info.sensor_size.y;
      case DIAGONAL:
        L = glm::length(render_info.sensor_size);
      default:
        L = render_info.sensor_size.y;
    }
    L = L / 2;
    return 2 * glm::atan(L / render_info.focal_length);
  }
  void setFocalDistanceByAov(AOV_TYPE type, float new_aov) {
    float tan_new_aov = glm::tan(new_aov / 2);
    float L = 0;
    switch (type) {
      case HORIZONTAL:
        L = render_info.sensor_size.x;
      case VERTICAL:
        L = render_info.sensor_size.y;
      case DIAGONAL:
        L = glm::length(render_info.sensor_size);
      default:
        L = render_info.sensor_size.y;
    }
    L = L / 2;
    render_info.focal_length = L / tan_new_aov;
  }
  glm::mat4 getProjectMatrix() {
    float aov_v = getAov(VERTICAL);
    glm::mat4 project_matrix;
    project_matrix = glm::perspectiveRH(aov_v, getAspect(), render_info.near, render_info.far);
    return project_matrix;
  }
};