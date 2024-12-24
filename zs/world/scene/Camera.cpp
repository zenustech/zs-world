#include "Camera.hpp"
#include <iostream>

void Camera::updatePlanes() {
  // view space view planes
  // left
  planeNormals[0] = glm::vec3(-1.0f, 0.0f, this->tanFOV.x);
  // right
  planeNormals[1] = glm::vec3(1.0f, 0.0f, this->tanFOV.x);
  // top
  planeNormals[2] = glm::vec3(0.0f, 1.0f, this->tanFOV.y);
  // bottom
  planeNormals[3] = glm::vec3(0.0f, -1.0f, this->tanFOV.y);
  // near
  planeNormals[4] = glm::vec3(0.0f, 0.0f, 1.0f);

  // transform normal from view space to world space
  const glm::mat4 invView = glm::transpose(matrices.view);
  for (int i = 0; i < 5; ++i) {
    glm::vec4 temp = invView * glm::vec4(planeNormals[i], 0.0f);
    planeNormals[i] = glm::normalize(glm::vec3(temp.x, temp.y, temp.z));
    planeOffsets[i] = glm::dot(planeNormals[i], -position);
  }
}

bool Camera::isSphereVisible(const glm::vec3& worldSpaceCenter, float radius) const {
  glm::vec4 viewCenter = matrices.view * glm::vec4(worldSpaceCenter, 1.0f);
  if (viewCenter.z - radius >= znear) return false;

  glm::vec2 refBorder = -viewCenter.z * tanFOV + radius * invCosFOV;
  if (viewCenter[0] < -refBorder[0] || viewCenter[0] > refBorder[0]) return false;
  if (viewCenter[1] < -refBorder[1] || viewCenter[1] > refBorder[1]) return false;
  return true;
}

bool Camera::isAABBVisible(const glm::vec3& minPos, const glm::vec3& maxPos) const {
  glm::vec3 nearPos;
  float dist;
  for (int i = 0; i < 4; ++i) {
    const auto& normal = planeNormals[i];
    nearPos.x = normal.x < 0.0f ? maxPos.x : minPos.x;
    nearPos.y = normal.y < 0.0f ? maxPos.y : minPos.y;
    nearPos.z = normal.z < 0.0f ? maxPos.z : minPos.z;
    dist = glm::dot(nearPos, normal);
    if (dist > planeOffsets[i]) {
      return false;
    }
  }
  return true; // -dist <= zfar; // easy far plane check
}
