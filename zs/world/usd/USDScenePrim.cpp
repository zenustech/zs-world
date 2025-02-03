#include "world/geometry/SimpleGeom.hpp"
#include "USDScenePrim.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "pxr/pxr.h"
#include "pxr/usd/sdf/copyUtils.h"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/primRange.h"
#include "pxr/usd/usd/relationship.h"
#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usdGeom/camera.h"
#include "pxr/usd/usdGeom/capsule.h"
#include "pxr/usd/usdGeom/cone.h"
#include "pxr/usd/usdGeom/cylinder.h"
#include "pxr/usd/usdGeom/mesh.h"
#include "pxr/usd/usdGeom/plane.h"
#include "pxr/usd/usdGeom/primvarsAPI.h"
#include "pxr/usd/usdGeom/sphere.h"
#include "pxr/usd/usdGeom/visibilityAPI.h"
#include "pxr/usd/usdGeom/xform.h"
#include "pxr/usd/usdGeom/xformCommonAPI.h"
#include "pxr/usd/usdGeom/xformOp.h"
#include "pxr/usd/usdGeom/xformable.h"
#include "pxr/usd/usdShade/material.h"
#include "pxr/usd/usdShade/materialBindingAPI.h"
#include "pxr/usd/usdShade/shader.h"
#include "pxr/usd/usdShade/udimUtils.h"
#include "pxr/usd/usdSkel/animQuery.h"
#include "pxr/usd/usdSkel/animation.h"
#include "pxr/usd/usdSkel/bindingAPI.h"
#include "pxr/usd/usdSkel/blendShapeQuery.h"
#include "pxr/usd/usdSkel/cache.h"
#include "pxr/usd/usdSkel/root.h"
#include "pxr/usd/usdSkel/skeleton.h"
#include "pxr/usd/usdSkel/skeletonQuery.h"
#include "pxr/usd/usdSkel/skinningQuery.h"
#include "pxr/usd/usdSkel/utils.h"

using vec3f = zs::vec<float, 3>;
using mat4f = zs::vec<float, 4, 4>;

namespace zs {
  static void _applyBlendShape(pxr::VtArray<pxr::GfVec3f>& points,
                               pxr::VtArray<pxr::GfVec3f>& normals,
                               pxr::UsdSkelBindingAPI& skelBinding,
                               pxr::UsdSkelSkeletonQuery& skelQuery,
                               pxr::UsdSkelSkinningQuery& skinQuery, double time) {
    if (!skinQuery.HasBlendShapes()) {
      return;
    }

    const pxr::UsdSkelAnimQuery& animQuery = skelQuery.GetAnimQuery();
    if (!animQuery) {
      return;
    }

    pxr::VtArray<float> weights;
    pxr::VtArray<float> realWeights;
    if (!animQuery.ComputeBlendShapeWeights(&weights, time)) {
      return;
    }

    if (skinQuery.GetBlendShapeMapper()) {
      if (!skinQuery.GetBlendShapeMapper()->Remap(weights, &realWeights)) {
        return;
      }
    } else {
      realWeights = std::move(weights);
    }

    pxr::UsdSkelBlendShapeQuery blendShapeQuery(skelBinding);
    pxr::VtArray<float> subShapeWeights;
    pxr::VtArray<unsigned int> blendShapeIndices;
    pxr::VtArray<unsigned int> subShapeIndices;
    if (!blendShapeQuery.ComputeSubShapeWeights(realWeights, &subShapeWeights, &blendShapeIndices,
                                                &subShapeIndices)) {
      return;
    }

    blendShapeQuery.ComputeDeformedPoints(
        pxr::TfMakeSpan(subShapeWeights), pxr::TfMakeSpan(blendShapeIndices),
        pxr::TfMakeSpan(subShapeIndices), blendShapeQuery.ComputeBlendShapePointIndices(),
        blendShapeQuery.ComputeSubShapePointOffsets(), pxr::TfMakeSpan(points));
    blendShapeQuery.ComputeDeformedNormals(pxr::TfMakeSpan(subShapeWeights), pxr::TfMakeSpan(blendShapeIndices),
      pxr::TfMakeSpan(subShapeIndices), blendShapeQuery.ComputeBlendShapePointIndices(),
      blendShapeQuery.ComputeSubShapePointOffsets(), pxr::TfMakeSpan(normals));
  }

  static void _applySkinning(pxr::VtArray<pxr::GfVec3f>& points,
                             pxr::VtArray<pxr::GfVec3f>& normals /* TODO */,
                             const pxr::UsdGeomMesh& mesh, double time) {
    pxr::UsdSkelCache skelCache;
    pxr::UsdSkelBindingAPI skelBinding(mesh.GetPrim());

    auto skelRoot = pxr::UsdSkelRoot::Find(mesh.GetPrim());
    if (!skelRoot) {
      // no skelRoot found for this mesh
      return;
    }

    skelCache.Populate(skelRoot, pxr::UsdTraverseInstanceProxies());
    pxr::UsdSkelSkeletonQuery skelQuery
        = skelCache.GetSkelQuery(skelBinding.GetInheritedSkeleton());
    pxr::UsdSkelSkinningQuery skinQuery = skelCache.GetSkinningQuery(mesh.GetPrim());

    pxr::VtArray<pxr::GfMatrix4d> skinningTransforms;
    if (!skelQuery.ComputeSkinningTransforms(&skinningTransforms, time)) {
      return;
    }

    _applyBlendShape(points, normals, skelBinding, skelQuery, skinQuery, time);

    if (!skinQuery.ComputeSkinnedPoints(skinningTransforms, &points, time)) {
      return;
    }

    pxr::GfMatrix4d bindTransform = skinQuery.GetGeomBindTransform(time).GetInverse();
    for (auto& point : points) {
      // point = bindTransform.Transform(point);
      auto tmp = bindTransform.Transform(point);
      point = pxr::GfVec3f{(float)tmp[0], (float)tmp[1], (float)tmp[2]};
    }

    // TODO: normals
    skinQuery.ComputeSkinnedNormals(skinningTransforms, &normals, time);
  }

  static inline pxr::GfVec3f _getVec3fFromOp(const pxr::UsdGeomXformOp& op, double time) {
    pxr::VtValue val;
    if (!op.Get(&val, time)) {
      return {0, 0, 0};
    }

    auto precision = op.GetPrecision();
    switch (precision) {
      case pxr::UsdGeomXformOp::PrecisionDouble:
        pxr::GfVec3d vd;
        op.Get(&vd);
        return pxr::GfVec3f(vd);
      case pxr::UsdGeomXformOp::PrecisionFloat:
        pxr::GfVec3f vf;
        op.Get(&vf);
        return vf;
      case pxr::UsdGeomXformOp::PrecisionHalf:
        pxr::GfVec3h vh;
        op.Get(&vh);
        return pxr::GfVec3f(vh);
      default:
        return {0, 0, 0};
    }
  }

  static inline bool _setVec3fToOp(pxr::UsdGeomXformOp& op, const pxr::GfVec3f& vec, double t) {
    auto precision = op.GetPrecision();
    if (precision == pxr::UsdGeomXformOp::PrecisionDouble) {
      return op.Set(pxr::GfVec3d(vec), t);
    } else if (precision == pxr::UsdGeomXformOp::PrecisionHalf) {
      return op.Set(pxr::GfVec3h(vec), t);
    }
    return op.Set(vec, t);
  }

  // get a float value from usd prim attribute
  static inline float _getFloatFromAttr(const pxr::UsdPrim& usdPrim, const char* attrName,
                                        double time) {
    double val;
    auto attr = usdPrim.GetAttribute(pxr::TfToken(attrName));
    if (!attr.Get(&val, time)) {
      return 0.0f;
    }
    return val;
  }

  // set  float value to usd prim attribute
  static inline bool _setFloatToAttr(pxr::UsdPrim& usdPrim, const char* attrName, float val,
                                     double time) {
    pxr::VtValue v(val);
    auto attr = usdPrim.GetAttribute(pxr::TfToken(attrName));
    if (!attr.IsValid()) {
      attr = usdPrim.CreateAttribute(pxr::TfToken(attrName), pxr::SdfGetValueTypeNameForValue(v));
    }
    if (!attr.IsValid()) return false;

    return attr.Set(v, time);
  }

  static inline char _getCharFromAttr(const pxr::UsdPrim& usdPrim, const char* attrName,
                                      double time) {
    pxr::TfToken c;
    auto attr = usdPrim.GetAttribute(pxr::TfToken(attrName));
    if (!attr.IsValid() || !attr.Get<pxr::TfToken>(&c, time) || c.size() == 0) {
      return '\0';
    }
    return c.GetString()[0];
  }

  static inline bool _setCharToAttr(pxr::UsdPrim& usdPrim, const char* attrName, char val,
                                    double time) {
    pxr::VtValue c(pxr::TfToken("" + val));
    auto attr = usdPrim.GetAttribute(pxr::TfToken(attrName));
    if (!attr.IsValid()) {
      attr = usdPrim.CreateAttribute(pxr::TfToken(attrName), pxr::SdfGetValueTypeNameForValue(c));
    }
    if (!attr.IsValid()) return false;

    return attr.Set(c, time);
  }

  // parse usd geometry to zpc mesh
  template <typename T, int dim, typename Tn>
  static bool _getTriMeshFromGeom(const std::string& typeName, const pxr::UsdPrim& usdPrim,
                                  Mesh<T, dim, Tn, 3>& triMesh, double time) {
    if (typeName == "Sphere") {
      triMesh = createSphere(_getFloatFromAttr(usdPrim, "radius", time));
    } else if (typeName == "Capsule") {
      triMesh = createCapsule(_getFloatFromAttr(usdPrim, "radius", time),
                              _getFloatFromAttr(usdPrim, "height", time),
                              _getCharFromAttr(usdPrim, "axis", time));
    } else if (typeName == "Cube") {
      triMesh = createCube(_getFloatFromAttr(usdPrim, "size", time));
    } else if (typeName == "Cylinder") {
      triMesh = createCylinder(_getFloatFromAttr(usdPrim, "radius", time),
                               _getFloatFromAttr(usdPrim, "height", time),
                               _getCharFromAttr(usdPrim, "axis", time));
    } else if (typeName == "Cone") {
      triMesh = createCone(_getFloatFromAttr(usdPrim, "radius", time),
                           _getFloatFromAttr(usdPrim, "height", time),
                           _getCharFromAttr(usdPrim, "axis", time));
    } else if (typeName == "Plane") {
      triMesh = createPlane(_getFloatFromAttr(usdPrim, "width", time),
                            _getFloatFromAttr(usdPrim, "length", time),
                            _getCharFromAttr(usdPrim, "axis", time));
    } else if (typeName == "Disk") {
      triMesh = createDisk(_getFloatFromAttr(usdPrim, "radius", time));
    } else {
      return false;
    }

    return true;
  }

  // parse usd mesh to zpc mesh
  template <typename T, int dim, typename Tn>
  static bool _getTriMeshFromMesh(const pxr::UsdPrim& usdPrim, Mesh<T, dim, Tn, 3>& triMesh,
                                  double time) {
    /*** Read from USD prim ***/
    const auto& usdMesh = pxr::UsdGeomMesh(usdPrim);
    auto extent = usdMesh.GetExtentAttr();  // bounding box

    bool isDoubleSided;
    usdMesh.GetDoubleSidedAttr().Get(&isDoubleSided);  // TODO: handle double sided meshes

    // decide whether we use left handed order to construct faces
    pxr::TfToken faceOrder;
    usdMesh.GetOrientationAttr().Get(&faceOrder, time);
    bool isReversedFaceOrder = (faceOrder.GetString() == "leftHanded");

    using Node = typename Mesh<T, dim, Tn, 3>::Node;
    using Elem = typename Mesh<T, dim, Tn, 3>::Elem;
    using Color = typename Mesh<T, dim, Tn, 3>::Color;
    using UV = typename Mesh<T, dim, Tn, 3>::UV;
    using Norm = typename Mesh<T, dim, Tn, 3>::Norm;

    const int baseVertIndex = triMesh.nodes.size();

    /*** Zeno Prim definition ***/
    auto& verts = triMesh.nodes;
    auto& faces = triMesh.elems;
    auto& uvs = triMesh.uvs;
    auto& norms = triMesh.norms;
    auto& colors = triMesh.colors;
    Node vert;
    Elem face;

    /*** Start setting up mesh ***/
    pxr::VtArray<pxr::GfVec3f> pointValues;
    usdMesh.GetPointsAttr().Get(&pointValues, time);
    _applySkinning(pointValues, pointValues, usdMesh, time);
    for (const auto& point : pointValues) {
      for (int i = 0; i < 3; ++i) {
        vert[i] = point.data()[i];
      }
      verts.emplace_back(vert);
    }

    auto vertColor = usdMesh.GetDisplayColorAttr();
    if (vertColor.HasValue()) {
      pxr::VtArray<pxr::GfVec3f> _cc;
      vertColor.Get(&_cc, time);
      if (_cc.size() == verts.size()) {
        Color color;
        for (const auto& c : _cc) {
          color = {c[0], c[1], c[2]};
          colors.emplace_back(color);
        }
      } else if (_cc.size() > 0) {
        Color color = {_cc[0][0], _cc[0][1], _cc[0][2]};
        for (int i = 0; i < verts.size(); ++i) {
          colors.emplace_back(color);
        }
      }
    }

    auto usdNormals = usdMesh.GetNormalsAttr();
    if (usdNormals.HasValue()) {
      pxr::VtArray<pxr::GfVec3f> normalValues;
      usdNormals.Get(&normalValues, time);
      Norm norm;
      for (const auto& normalValue : normalValues) {
        const float* ptr = normalValue.data();
        norm = {ptr[0], ptr[1], ptr[2]};
        norms.emplace_back(norm);
      }
    }

    // constructing faces, treat all meshes as poly mesh
    pxr::VtArray<int> faceSizeValues;  // numbers of vertices for each face
    pxr::VtArray<int> usdIndices;
    usdMesh.GetFaceVertexCountsAttr().Get(&faceSizeValues, time);
    usdMesh.GetFaceVertexIndicesAttr().Get(&usdIndices, time);
    int faceIndexStart = 0;
    for (int faceSize : faceSizeValues) {
      if (faceSize <= 2) {
        std::cout << "found face size smaller than 3, skip for now: " << faceSize << std::endl;
        faceIndexStart += faceSize;
        continue;
      }

      // Decomposing poly into triangles
      face[0] = usdIndices[faceIndexStart];
      for (int i = 1; i + 1 < faceSize; ++i) {
        if (isReversedFaceOrder) {
          face[2] = baseVertIndex + usdIndices[faceIndexStart + i];
          face[1] = baseVertIndex + usdIndices[faceIndexStart + i + 1];
        } else {
          face[1] = baseVertIndex + usdIndices[faceIndexStart + i];
          face[2] = baseVertIndex + usdIndices[faceIndexStart + i + 1];
        }
        faces.emplace_back(face);
      }
      faceIndexStart += faceSize;
    }

    /*
    auto vertUVs = usdPrim.GetAttribute(pxr::TfToken("primvars:st"));
    if (vertUVs.HasValue()) {
        pxr::VtArray<pxr::GfVec2f> usdUVs;
        vertUVs.Get(&usdUVs);

        UV uv;
        for (int i = 0; i < usdUVs.size(); ++i) {
            uv = { usdUV[i][0], usdUV[i][1] };
            uvs.emplace_back(uv);
        }

        auto uvIndicesAttr = usdPrim.GetAttribute(pxr::TfToken("primvars:st:indices"));
        if (uvIndicesAttr.HasValue()) {
            pxr::VtArray<int> usdUVIndices;
            uvIndicesAttr.Get(&usdUVIndices);

            if (usdUVIndices.size() == loops.size()) { // uv index size matches vertex index size
                auto& uvIndices = zPrim.loops.add_attr<int>("uvs");
                for (int i = 0; i < loops.size(); ++i) {
                    uvIndices[i] = usdUVIndices[i];
                }
            } else if (usdUVIndices.size() == verts.size()) { // uv index size matches vertex size
                auto& uvIndices = zPrim.loops.add_attr<int>("uvs");
                for (int i = 0; i < loops.size(); ++i) {
                    int refVertIndex = loops[i];
                    uvIndices[i] = usdUVIndices[refVertIndex];
                }
            } else {
                zeno::log_error("found incorrect number of st:indices {} from prim {}",
    usdUVIndices.size(), usdPrim.GetPath().GetString());
            }
        } else {
            if (usdUVs.size() == loops.size()) {
                auto& uvIndices = zPrim.loops.add_attr<int>("uvs");
                for (int i = 0; i < loops.size(); ++i) {
                    uvIndices[i] = i;
                }
            } else if (usdUVs.size() == verts.size()) {
                auto& uvIndices = zPrim.loops.add_attr<int>("uvs");
                for (int i = 0; i < loops.size(); ++i) {
                    int refVertIndex = loops[i];
                    uvIndices[i] = refVertIndex;
                }
            } else {
                zeno::log_error("invalid st size for mesh: {} from prim {}", usdUVs.size(),
    usdMesh.GetPath().GetString());
            }
        }
    }*/

    return true;
  }

  // parse USD geometry or mesh to zpc mesh
  template <typename T, int dim, typename Tn>
  static bool _getTriMesh(const pxr::UsdPrim& usdPrim, Mesh<T, dim, Tn, 3>& triMesh, double time) {
    const std::string& typeName = usdPrim.GetTypeName().GetString();

    if (typeName != "Mesh") {
      return _getTriMeshFromGeom(typeName, usdPrim, triMesh, time);
    } else {
      return _getTriMeshFromMesh(usdPrim, triMesh, time);
    }
  }

  /******************
   * zeno-USD Prim
   *******************/
  USDScenePrim::USDScenePrim() : mScene(nullptr) {}
  USDScenePrim::USDScenePrim(SceneDescConcept* scene, std::any usdPrim)
      : mScene(scene), mPrim(usdPrim) {}

  bool USDScenePrim::isValid() const {
    if (!mScene || !mPrim.has_value()) {
      return false;
    }
    return std::any_cast<pxr::UsdPrim>(mPrim).IsValid();
  }

  ScenePrimHolder USDScenePrim::getParent() const {
    auto p = std::any_cast<pxr::UsdPrim>(mPrim);
    auto parent = p.GetParent();

    if (!parent.IsValid()) {
      return ScenePrimHolder();
    }

    return ScenePrimHolder(new USDScenePrim(mScene, parent));
  }

  ScenePrimHolder USDScenePrim::getChild(const char* childName) const {
    auto p = std::any_cast<pxr::UsdPrim>(mPrim);
    auto childPrim = p.GetChild(pxr::TfToken(childName));
    if (!childPrim.IsValid()) {
      return ScenePrimHolder();
    }

    return ScenePrimHolder(new USDScenePrim(mScene, childPrim));
  }

  ScenePrimHolder USDScenePrim::addChild(const char* childName) {
    auto p = std::any_cast<pxr::UsdPrim>(mPrim);
    std::string childPath = p.GetPath().GetString() + "/" + childName;
    auto childPrim = p.GetStage()->DefinePrim(pxr::SdfPath(childPath));
    if (!childPrim.IsValid()) {
      return ScenePrimHolder();
    }

    return ScenePrimHolder(new USDScenePrim(mScene, childPrim));
  }

  bool USDScenePrim::getAllChilds(size_t* num, ScenePrimHolder* childs) const {
    if (!isValid() || !num) {
      return false;
    }

    auto prim = std::any_cast<pxr::UsdPrim>(mPrim);
    auto range = prim.GetChildren();

    // there is no function like range.size() to get the number of children, so we traverse it
    int counter = 0;
    for (auto child : range) {
      if (childs) {
        childs[counter] = ScenePrimHolder(new USDScenePrim(mScene, child.GetPrim()));
      }
      ++counter;
    }

    *num = counter;
    return true;
  }

  SceneDescConcept* USDScenePrim::getScene() const { return const_cast<SceneDescConcept*>(mScene); }

  const char* USDScenePrim::getName() const {
    if (!isValid()) return "";

    auto prim = std::any_cast<pxr::UsdPrim>(mPrim);
    return prim.GetName().GetString().c_str();
  }

  const char* USDScenePrim::getPath() const {
    if (!isValid()) return "";

    auto prim = std::any_cast<pxr::UsdPrim>(mPrim);
    return prim.GetPath().GetString().c_str();
  }

  const char* USDScenePrim::getTypeName() const {
    if (!isValid()) return "";

    auto prim = std::any_cast<pxr::UsdPrim>(mPrim);
    return prim.GetTypeName().GetString().c_str();
  }

  bool USDScenePrim::getVisible(double time) const {
    if (!isValid()) return false;

    auto prim = std::any_cast<pxr::UsdPrim>(mPrim);
    auto vis = pxr::UsdGeomVisibilityAPI(prim);
    bool visible;
    vis.GetRenderVisibilityAttr().Get<bool>(&visible, time);
    return visible;
  }

  bool USDScenePrim::setVisible(bool visible, double time) {
    if (!isValid()) return false;

    auto prim = std::any_cast<pxr::UsdPrim>(mPrim);
    auto vis = pxr::UsdGeomVisibilityAPI(prim);
    return vis.GetRenderVisibilityAttr().Set(visible, time);
  }

  bool USDScenePrim::setLocalMatrixOpValue(const glm::mat4& transform, double time, const std::string& suffix) {
    if (!isValid()) return false;

    pxr::GfMatrix4d mat;
    for (int i = 0; i < 16; ++i) {
      mat[i >> 2][i & 3] = static_cast<double>(transform[i >> 2][i & 3]);
    }
    auto xform = pxr::UsdGeomXformable(std::any_cast<pxr::UsdPrim>(mPrim));
    auto op = xform.GetTransformOp(pxr::TfToken(suffix));
    if (!op.IsDefined()) {
      op = xform.AddTransformOp(pxr::UsdGeomXformOp::PrecisionDouble, pxr::TfToken(suffix));
    }
    bool result = op.Set(mat, time);
    if (result) {
      mScene->markZpcTransformDirty();
    }
    return result;
  }

  bool USDScenePrim::setLocalTranslate(const glm::vec3& translation, double time) {
    if (!isValid()) return false;

    pxr::GfVec3f t;
    t.Set(translation[0], translation[1], translation[2]);
    auto xform = pxr::UsdGeomXformable(std::any_cast<pxr::UsdPrim>(mPrim));
    auto op = xform.GetTranslateOp();
    if (!op.IsDefined()) {
      op = xform.AddTranslateOp(pxr::UsdGeomXformOp::PrecisionFloat);
    }
    return _setVec3fToOp(op, t, time);
  }

  bool USDScenePrim::setLocalScale(const glm::vec3& scale, double time) {
    if (!isValid()) return false;

    pxr::GfVec3f s;
    s.Set(scale[0], scale[1], scale[2]);
    auto xform = pxr::UsdGeomXformable(std::any_cast<pxr::UsdPrim>(mPrim));
    auto op = xform.GetScaleOp();
    if (!op.IsDefined()) {
      op = xform.AddScaleOp(pxr::UsdGeomXformOp::PrecisionFloat);
    }
    return _setVec3fToOp(op, s, time);
  }

  bool USDScenePrim::setLocalRotateXYZ(const glm::vec3& euler, double time) {
    if (!isValid()) return false;

    pxr::GfVec3f r;
    r.Set(euler[0], euler[1], euler[2]);
    auto xform = pxr::UsdGeomXformable(std::any_cast<pxr::UsdPrim>(mPrim));
    auto op = xform.GetRotateXYZOp();
    if (!op.IsDefined()) {
      op = xform.AddRotateXYZOp(pxr::UsdGeomXformOp::PrecisionFloat);
    }
    return _setVec3fToOp(op, r, time);
  }

  glm::mat4 USDScenePrim::getLocalMatrixOpValue(double time, const std::string& suffix) const {
    if (!isValid()) return {};

    glm::mat4 mat(0.0f);
    mat[0][0] = mat[1][1] = mat[2][2] = mat[3][3] = 1.0f;

    auto xform = pxr::UsdGeomXformable(std::any_cast<pxr::UsdPrim>(mPrim));
    auto op = xform.GetTransformOp(pxr::TfToken(suffix));
    if (op.IsDefined()) {
      pxr::GfMatrix4d m;
      if (op.Get(&m, time)) {
        for (int i = 0; i < 16; ++i) {
          mat[i >> 2][i & 3] = m[i >> 2][i & 3];
        }
      }
    }
    return mat;
  }

  glm::mat4 USDScenePrim::getLocalTransform(double time) const {
    if (!isValid()) return {};

    glm::mat4 mat(0.0f);
    mat[0][0] = mat[1][1] = mat[2][2] = mat[3][3] = 1.0f;

    auto xform = pxr::UsdGeomXformable(std::any_cast<pxr::UsdPrim>(mPrim));
    if (xform) {
      pxr::GfMatrix4d m;
      bool resetFromStack;
      if (xform.GetLocalTransformation(&m, &resetFromStack, time)) {
        for (int i = 0; i < 4; ++i) {
          for (int j = 0; j < 4; ++j) {
            mat[i][j] = m[i][j];
          }
        }
      }
    }

    return mat;
  }

  glm::vec3 USDScenePrim::getLocalTranslate(double time) const {
    if (!isValid()) return {};

    glm::vec3 ret;
    ret[0] = ret[1] = ret[2] = 0.0f;

    auto xform = pxr::UsdGeomXformable(std::any_cast<pxr::UsdPrim>(mPrim));
    auto op = xform.GetTranslateOp();
    if (op.IsDefined()) {
      pxr::GfVec3f t = _getVec3fFromOp(op, time);
      for (int i = 0; i < 3; ++i) {
        ret[i] = t[i];
      }
    }

    return ret;
  }

  glm::vec3 USDScenePrim::getLocalScale(double time) const {
    if (!isValid()) return {};

    glm::vec3 ret;
    ret[0] = ret[1] = ret[2] = 1.0f;

    auto xform = pxr::UsdGeomXformable(std::any_cast<pxr::UsdPrim>(mPrim));
    auto op = xform.GetScaleOp();
    if (op.IsDefined()) {
      pxr::GfVec3f t = _getVec3fFromOp(op, time);
      for (int i = 0; i < 3; ++i) {
        ret[i] = t[i];
      }
    }

    return ret;
  }

  glm::vec3 USDScenePrim::getLocalRotateXYZ(double time) const {
    if (!isValid()) return {};

    glm::vec3 ret;
    ret[0] = ret[1] = ret[2] = 0.0f;

    auto xform = pxr::UsdGeomXformable(std::any_cast<pxr::UsdPrim>(mPrim));
    auto op = xform.GetRotateXYZOp();
    if (op.IsDefined()) {
      pxr::GfVec3f t = _getVec3fFromOp(op, time);
      for (int i = 0; i < 3; ++i) {
        ret[i] = t[i];
      }
    }

    return ret;
  }

  bool USDScenePrim::isTimeVarying() const {
    if (!isValid()) return false;

    auto xform = pxr::UsdGeomXformable(std::any_cast<pxr::UsdPrim>(mPrim));
    return xform.TransformMightBeTimeVarying();
  }

  bool USDScenePrim::getSkelAnimTimeCodeInterval(double& start, double& end) const {
    if (!isValid()) return false;
    auto prim = std::any_cast<pxr::UsdPrim>(mPrim);
    auto skelRoot = pxr::UsdSkelRoot::Find(prim);
    if (!skelRoot) return false;

    std::vector<double> timeSamples;
    if (!skelRoot.GetTimeSamples(&timeSamples) || timeSamples.size() == 0) return false;

    start = timeSamples[0];
    end = timeSamples[timeSamples.size() - 1];
    return true;
  }

#define EASY_IMPLEMENT_TIME_SAMPLES(name, from_type, attr_getter)          \
  size_t USDScenePrim::get##name##TimeSamples(double* timeSamples) const { \
    if (!isValid()) return 0;                                              \
    auto target = pxr::from_type(std::any_cast<pxr::UsdPrim>(mPrim));      \
    if (!target) return 0;                                                 \
    auto attr = target.Get##attr_getter();                                 \
    if (!attr) return 0;                                                   \
    size_t num = attr.GetNumTimeSamples();                                 \
    if (num > 0 && timeSamples) {                                          \
      std::vector<double> times(num);                                      \
      attr.GetTimeSamples(&times);                                         \
      memcpy(timeSamples, times.data(), sizeof(double) * num);             \
    }                                                                      \
    return num;                                                            \
  }

  EASY_IMPLEMENT_TIME_SAMPLES(Visible, UsdGeomVisibilityAPI, RenderVisibilityAttr);
  EASY_IMPLEMENT_TIME_SAMPLES(FaceIndex, UsdGeomMesh, FaceVertexIndicesAttr);
  EASY_IMPLEMENT_TIME_SAMPLES(FaceSize, UsdGeomMesh, FaceVertexCountsAttr);
  EASY_IMPLEMENT_TIME_SAMPLES(Point, UsdGeomMesh, PointsAttr);
  EASY_IMPLEMENT_TIME_SAMPLES(Normal, UsdGeomMesh, NormalsAttr);
  EASY_IMPLEMENT_TIME_SAMPLES(Color, UsdGeomMesh, DisplayColorAttr);
  EASY_IMPLEMENT_TIME_SAMPLES(Translation, UsdGeomXformable, TranslateOp);
  EASY_IMPLEMENT_TIME_SAMPLES(Scale, UsdGeomXformable, ScaleOp);
  EASY_IMPLEMENT_TIME_SAMPLES(RotationX, UsdGeomXformable, RotateXOp);
  EASY_IMPLEMENT_TIME_SAMPLES(RotationY, UsdGeomXformable, RotateYOp);
  EASY_IMPLEMENT_TIME_SAMPLES(RotationZ, UsdGeomXformable, RotateZOp);
  EASY_IMPLEMENT_TIME_SAMPLES(RotationXYZ, UsdGeomXformable, RotateXYZOp);
  EASY_IMPLEMENT_TIME_SAMPLES(RotationXZY, UsdGeomXformable, RotateXZYOp);
  EASY_IMPLEMENT_TIME_SAMPLES(RotationYXZ, UsdGeomXformable, RotateYXZOp);
  EASY_IMPLEMENT_TIME_SAMPLES(RotationYZX, UsdGeomXformable, RotateYZXOp);
  EASY_IMPLEMENT_TIME_SAMPLES(RotationZXY, UsdGeomXformable, RotateZXYOp);
  EASY_IMPLEMENT_TIME_SAMPLES(RotationZYX, UsdGeomXformable, RotateZYXOp);
  EASY_IMPLEMENT_TIME_SAMPLES(MatrixOp, UsdGeomXformable, TransformOp);

  /*
   * USD doesn't provide GetNumTimeSamples in Primvar
   * so we have to GetTimeSamples and get size() to know how large is the time samples :)
   */
  size_t USDScenePrim::getUVTimeSamples(double* timeSamples) const {
    if (!isValid()) return 0;
    auto target = pxr::UsdGeomPrimvarsAPI(std::any_cast<pxr::UsdPrim>(mPrim));
    if (!target) return 0;

    size_t num = 0;
    auto attr
        = target.GetPrimvar(pxr::TfToken("st"));  // TODO: consider multi-uv (st0, st1, st2...)
    std::vector<double> times;
    if (attr && attr.GetTimeSamples(&times)) {
      num = times.size();
      if (timeSamples) {
        memcpy(timeSamples, times.data(), sizeof(double) * num);
      }
    }
    return num;
  }

#undef EASY_IMPLEMENT_TIME_SAMPLES

  std::any USDScenePrim::getRawPrim() const { return mPrim; }

  bool USDScenePrim::copy(const ScenePrimConcept* other) {
    if (!other || !isValid()) return false;

    auto src = std::any_cast<pxr::UsdPrim>(other->getRawPrim());
    auto dst = std::any_cast<pxr::UsdPrim>(mPrim);

    auto srcPath = src.GetPath();
    auto dstPath = dst.GetPath();

    bool ret = false;
    for (auto spec : src.GetPrimStack()) {
      const auto& layer = spec->GetLayer();
      // TODO: SdfCopySpec seems not working
      ret |= pxr::SdfCopySpec(layer, srcPath, layer, dstPath);
    }

    ret |= dst.SetTypeName(src.GetTypeName());

    for (auto metaIt : src.GetAllAuthoredMetadata()) {
      ret |= dst.SetMetadata(metaIt.first, metaIt.second);
    }

    // copying relationships
    for (const auto& relation : src.GetRelationships()) {
      pxr::SdfPathVector targets;
      relation.GetTargets(&targets);
      if (targets.empty()) continue;

      for (auto& target : targets) {
        target = dstPath.AppendPath(target.MakeRelativePath(srcPath));
      }

      const pxr::TfToken& name = relation.GetName();
      if (dst.HasRelationship(name)) {
        ret |= dst.GetRelationship(name).SetTargets(targets);
      } else {
        auto rel = dst.CreateRelationship(name);
        ret |= rel.SetTargets(targets);
      }
    }

    // copying attributes
    pxr::VtValue val;
    std::vector<double> times;
    for (const auto& attr : src.GetAttributes()) {
      pxr::TfToken attrName = attr.GetName();
      attr.GetTimeSamples(&times);

      pxr::UsdAttribute myAttr;
      if (dst.HasAttribute(attrName)) {
        myAttr = dst.GetAttribute(attrName);
      } else {
        myAttr = dst.CreateAttribute(attrName, attr.GetTypeName(), attr.GetVariability());
      }

      if (!times.empty()) {
        for (double time : times) {
          if (!attr.Get(&val, time)) continue;
          ret |= myAttr.Set(val, time);
        }
      } else {
        if (!attr.Get(&val)) continue;
        ret |= myAttr.Set(val);
      }
    }

    return ret;
  }

  bool USDScenePrim::copy(const ScenePrimHolder& other) { return copy(other.get()); }

  bool USDScenePrim::fromVisMesh(const Mesh<float, 3, u32, 3>& mesh) {
    // TODO: transform matrix and time
    auto prim = std::any_cast<pxr::UsdPrim>(mPrim);
    if (!prim.IsValid()) {
      return false;
    }
    if (!prim.SetTypeName(pxr::TfToken("Mesh"))) {
      return false;
    }

    auto me = pxr::UsdGeomMesh(prim);

    using Node = typename Mesh<float, 3, u32, 3>::Node;
    using Elem = typename Mesh<float, 3, u32, 3>::Elem;
    using Color = typename Mesh<float, 3, u32, 3>::Color;
    using UV = typename Mesh<float, 3, u32, 3>::UV;
    using Norm = typename Mesh<float, 3, u32, 3>::Norm;

    const auto& verts = mesh.nodes;
    const auto& faces = mesh.elems;
    const auto& colors = mesh.colors;
    const auto& norms = mesh.norms;
    const auto& uvs = mesh.uvs;  // TODO

#define USD_MESH_ATTR(x, suffix) \
  auto x = me.Get##suffix();     \
  if (!x.IsValid()) {            \
    x = me.Create##suffix();     \
  }                              \
  x.Clear();

    // Positions
    USD_MESH_ATTR(points, PointsAttr);
    pxr::VtArray<pxr::GfVec3f> vertArray;
    for (const auto& vert : verts) {
      vertArray.emplace_back(vert[0], vert[1], vert[2]);
    }
    points.Set(vertArray);

    // Indexing
    USD_MESH_ATTR(usdFaces, FaceVertexIndicesAttr);
    USD_MESH_ATTR(usdFaceSizes, FaceVertexCountsAttr);
    pxr::VtArray<int> indexArray;
    pxr::VtArray<int> faceSizeArray;
    for (auto face : faces) {
      indexArray.emplace_back(face[0]);
      indexArray.emplace_back(face[1]);
      indexArray.emplace_back(face[2]);
    }
    usdFaces.Set(indexArray);
    faceSizeArray.assign(faces.size(), 3);
    usdFaceSizes.Set(faceSizeArray);

    // Colors
    if (colors.size() > 0) {
      USD_MESH_ATTR(usdColors, DisplayColorAttr);

      pxr::VtArray<pxr::GfVec3f> colorArray;
      for (const auto& col : colors) {
        colorArray.emplace_back(col[0], col[1], col[2]);
      }
      usdColors.Set(colorArray);
    }

    // Normals
    if (norms.size() > 0) {
      USD_MESH_ATTR(usdNorms, NormalsAttr);

      pxr::VtArray<pxr::GfVec3f> normalArray;
      for (const auto& norm : norms) {
        normalArray.emplace_back(norm[0], norm[1], norm[2]);
      }
      usdNorms.Set(normalArray);
    }
#undef USD_MESH_ATTR
    return true;
  }

  Mesh<float, 3, u32, 3> USDScenePrim::toVisMesh(double time) const {
    if (!isValid()) {
      return {};
    }
    auto prim = std::any_cast<pxr::UsdPrim>(mPrim);

    Mesh<float, 3, u32, 3> triMesh;

    // converting mesh
    if (!_getTriMesh<float, 3, u32>(prim, triMesh, time)) {
      return {};
    }

    glm::mat4 trans = getWorldTransform(time);
    // coordinate system transform
    auto C = glm::mat4();
    C[0][0] = 1.f;
    C[2][1] = 1.f;
    C[1][2] = -1.f;
    C[3][3] = 1.f;
    trans = C * trans;
    mat4f mat;
    for (int i = 0; i < 4; ++i) {
      mat[i][0] = trans[i][0];
      mat[i][1] = trans[i][1];
      mat[i][2] = trans[i][2];
      mat[i][3] = trans[i][3];
    }
    for (int i = 0; i < triMesh.nodes.size(); ++i) {
      auto& node = triMesh.nodes[i];
      glm::vec4 p{node[0], node[1], node[2], 1.f};
      p = trans * p;
      node[0] = p[0];
      node[1] = p[1];
      node[2] = p[2];
    }

    return triMesh;
  }

  glm::mat4 USDScenePrim::getWorldTransform(double time) const {
    if (!isValid()) return {};

    auto prim = std::any_cast<pxr::UsdPrim>(mPrim);
    pxr::UsdGeomXform xform(prim);
    pxr::GfMatrix4d _trans = xform.ComputeLocalToWorldTransform(time);
    double* ptr = _trans.data();
    glm::mat4 trans;
    for (int i = 0; i < 16; ++i) {
      trans[i >> 2][i & 3] = ptr[i];
    }
    return trans;
  }
}  // namespace zs
