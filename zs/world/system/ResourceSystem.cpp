#include "ResourceSystem.hpp"

#include <filesystem>

#include "world/World.hpp"
//
#include "world/scene/PrimitiveConversion.hpp"
//
#include "zensim/vulkan/Vulkan.hpp"
#include "zensim/zpc_tpls/fmt/format.h"

//
#define STB_IMAGE_IMPLEMENTATION
#include "zensim/zpc_tpls/stb/stb_image.h"

namespace zs {

  const std::string ResourceSystem::_missingTextureRelPath
      = "/resource/textures/missing_texture.png";

  static ResourceSystem *g_instance{nullptr};
  ResourceSystem::ResourceSystem() {
    // e.g. "/tmp, C:/tmp"
    _rootCachePath = std::filesystem::temp_directory_path().string();

    auto fn = zs::abs_exe_directory() + "/resource/scripts/" + g_textEditorFile;
    _scripts[g_textEditorLabel] = Archive{fn};
    _scripts[g_textEditorLabel].loadFromFileAscii(fn);

    _sceneContexts[g_defaultSceneLabel] = SceneContext();

#if ZS_ENABLE_USD
    /// @note not working as expected for now.
    _usdObserver.onSceneUpdate = [&](const SceneDescConcept *scene) {
      /// not sure if through
      // ZS_EVENT_SCHEDULER().emplace([]() {});
      // instance()._usdEntries[scene->getName()].markDirty();
      // onUsdFilesChanged().emit({std::string{scene->getName()}});
      // fmt::print("scene {} is modified!\n", scene->getName());
    };
    // zs_get_scene_manager(zs_get_world())->addObserver(&_usdObserver);
#endif
  }
  ResourceSystem::~ResourceSystem() { reset(); }
  ResourceSystem &ResourceSystem::instance() {
    if (!g_instance) g_instance = new ResourceSystem();
    return *g_instance;
  }
  void ResourceSystem::reset() {
    for (auto &icon : instance()._iconTextures) icon.reset();
    for (auto &set : instance()._iconDescriptorSets) set = vk::DescriptorSet{};
    // widgets
    instance()._widgets.clear();
    // textures
    instance()._textures.clear();
    instance()._textureDescriptorSets.clear();
    instance()._missingTexture.reset();
    instance()._missingTextureDescriptorSet = vk::DescriptorSet{};
    // shaders
    instance()._shaders.clear();
    // scene contexts
    instance()._sceneContexts.clear();
    // scripts
    {
      std::vector<std::string> scriptNames = get_script_labels();
      for (auto &label : scriptNames) {
        close_script(label);
      }
      instance()._scripts.clear();  // close script files
    }
    // usd
#if ZS_ENABLE_USD
    {
      std::vector<std::string> usdNames = get_usd_labels();
      for (auto &label : usdNames) {
        close_usd(label);
      }
      instance()._usdEntries.clear();
    }
#endif
  }
  void ResourceSystem::initialize() { (void)instance(); }

  std::string ResourceSystem::root_directory() { return abs_module_directory(); }
  std::string ResourceSystem::cache_directory() { return root_directory() + "/.cache"; }

  void ResourceSystem::register_image_for_gui(void *s) { instance()._registeredSets.insert(s); }
  bool ResourceSystem::image_avail_for_gui(void *s) {
    return instance()._registeredSets.find(s) != instance()._registeredSets.end();
  }
  vk::DescriptorSet &ResourceSystem::get_icon_descr_set(icon_e id) {
    return instance()._iconDescriptorSets[static_cast<size_t>(id)];
  }
  VkTexture &ResourceSystem::get_icon_texture(icon_e id) {
    return instance()._iconTextures[static_cast<size_t>(id)];
  }

  bool ResourceSystem::update_icon_texture(VulkanContext &ctx, icon_e id, std::string file) {
    auto &inst = instance();
#if RESOURCE_AT_RELATIVE_PATH
    file = abs_exe_directory() + "/resource/icons/" + std::string(file);
#else
    file = std::string{AssetDirPath} + "/resource/icons/" + std::string(file);
#endif
    fmt::print("loading icon texture [{}]...\t", file.data());
    int w, h, nchns;
    unsigned char *img = stbi_load(file.data(), &w, &h, &nchns, STBI_rgb_alpha);
    if (!img) {
      throw std::runtime_error(fmt::format("loading texture [{}] failed", file.data()));
    }

    if (w > 0 && h > 0) {
      inst._iconTextures[(int)id]
          = zs::load_texture(ctx, img, (size_t)w * h * STBI_rgb_alpha, vk::Extent2D{(u32)w, (u32)h},
                             vk::Format::eR8G8B8A8Unorm);

      auto &set = inst._iconDescriptorSets[(int)id];
      ctx.acquireSet(get_shader("imgui.frag").layout(0), set);
      zs::DescriptorWriter writer{ctx, get_shader("imgui.frag").layout(0)};
      vk::DescriptorImageInfo imgInfo = inst._iconTextures[(int)id].descriptorInfo();
      writer.writeImage(0, &imgInfo);
      writer.overwrite(set);
      register_image_for_gui(&set);
      //
      // ctx.registerImage(inst._iconTextures[(int)id]);
    }

    stbi_image_free(img);
    fmt::print("done.\n");
    return true;
  }

  bool ResourceSystem::load_texture(VulkanContext &ctx, const std::string &label,
                                    const ShaderModule &bind_shader, int layout_set /* = 0 */,
                                    int binding /* = 0 */
  ) {
    fmt::print("loading texture at {}\n", label.data());

    int w = 0, h = 0, nchns;
    unsigned char *img = stbi_load(label.data(), &w, &h, &nchns, STBI_rgb_alpha);
    if (!img) {
      fmt::print("failed to load missing texture {}\n", label.data());
      return false;
    }
    if (w == 0 || h == 0) {
      return false;
    }

    auto &texEntry = instance()._textures[label];
    auto &tex = texEntry.tex;
    tex = zs::load_texture(ctx, img, (size_t)w * h * STBI_rgb_alpha, vk::Extent2D{(u32)w, (u32)h},
                           vk::Format::eR8G8B8A8Unorm);

    // TODO: shader name should be related to material
    auto &set = instance()._textureDescriptorSets[label];
    ctx.acquireSet(bind_shader.layout(layout_set), set);
    zs::DescriptorWriter writer{ctx, bind_shader.layout(layout_set)};
    vk::DescriptorImageInfo imgInfo = tex.descriptorInfo();
    writer.writeImage(binding, &imgInfo);
    writer.overwrite(set);
    texEntry.bindlessId = ctx.registerImage(tex);

    stbi_image_free(img);

    onTexturesAdded().emit({label});

    return true;
  }
  bool ResourceSystem::load_missing_texture(VulkanContext &ctx, const ShaderModule &bind_shader,
                                            int layout_set /* = 0 */, int binding /* = 0 */
  ) {
#if RESOURCE_AT_RELATIVE_PATH
    const std::string path = abs_exe_directory() + _missingTextureRelPath;
#else
    const std::string path = std::string{AssetDirPath} + _missingTextureRelPath;
#endif
    fmt::print("loading missing texture at {}\n", path.data());

    int w = 0, h = 0, nchns;
    unsigned char *img = stbi_load(path.data(), &w, &h, &nchns, STBI_rgb_alpha);
    if (!img) {
      fmt::print("failed to load missing texture {}\n", path.data());
      return false;
    }
    if (w == 0 || h == 0) {
      return false;
    }

    auto &tex = instance()._missingTexture.tex;
    tex = zs::load_texture(ctx, img, (size_t)w * h * STBI_rgb_alpha, vk::Extent2D{(u32)w, (u32)h},
                           vk::Format::eR8G8B8A8Unorm);

    // TODO: shader name should be related to material
    auto &set = instance()._missingTextureDescriptorSet;
    ctx.acquireSet(bind_shader.layout(layout_set), set);
    zs::DescriptorWriter writer{ctx, bind_shader.layout(layout_set)};
    vk::DescriptorImageInfo imgInfo = tex.descriptorInfo();
    writer.writeImage(binding, &imgInfo);
    writer.overwrite(set);
    instance()._missingTexture.bindlessId = ctx.registerImage(tex);

    stbi_image_free(img);
    return true;
  }
  bool ResourceSystem::has_texture_loaded(const std::string &label) {
    return instance()._textureDescriptorSets.contains(label);
  }
  VkTexture &ResourceSystem::get_texture(const std::string &label) {
    if (instance()._textures.contains(label)) {
      return instance()._textures.at(label).tex;
    }
    return instance()._missingTexture.tex;
  }
  const vk::DescriptorSet &ResourceSystem::get_texture_descriptor_set(const std::string &label) {
    if (instance()._textureDescriptorSets.contains(label)) {
      return instance()._textureDescriptorSets[label];
    }
    return instance()._missingTextureDescriptorSet;
  }
  const vk::DescriptorSet &ResourceSystem::get_missing_texture_descriptor_set() {
    return instance()._missingTextureDescriptorSet;
  }

  bool ResourceSystem::load_shader(VulkanContext &ctx, const std::string &label,
                                   vk::ShaderStageFlagBits stage, std::string_view code,
                                   const source_location &loc) {
    auto &inst = instance();
    auto &shader = inst._shaders[label];
    if (shader) {
      fmt::print("shader [{}] already exists!\n", label);
      return false;
    }
    shader
        = ctx.createShaderModuleFromGlsl(code.data(), stage,
                                         fmt::format("file[{}]:line[{}]:func[{}]", loc.file_name(),
                                                     loc.line(), loc.function_name()));
    return true;
  }
  ShaderModule &ResourceSystem::get_shader(const std::string &label) {
    return instance()._shaders.at(label).get();
  }

  bool ResourceSystem::load_script(std::string_view filename, std::string_view label) {
    auto &inst = instance();
    auto &script = inst._scripts[std::string(filename)];
    if (script.valid()) {
      fmt::print("script [{}] already exists! reloading!\n", filename);
    }
    Archive archive;
    int ret = archive.loadFromFileAscii(filename) == 0;
    inst._scripts[std::string(label)] = zs::move(archive);
    onScriptFilesOpened().emit({std::string(label)});
    return ret;
  }

#if ZS_ENABLE_USD
  SceneDescConcept *ResourceSystem::load_usd(std::string_view filename, std::string_view label) {
    auto sceneMgr = zs_get_scene_manager(zs_get_world());

    zs::SceneConfig conf;
    conf.setString("srcPath", filename.data());

    if (auto ret = sceneMgr->getScene(label.data()); ret) {
      assert(instance()._usdEntries.contains(label));
      return ret;
    }

    auto scene = sceneMgr->createScene(label.data());
    // fmt::print("load usd label [{}] scene addr [{}]\n", label, (void *)scene);
    std::string l{label};
    if (scene->openScene(conf)) {
      instance()._usdEntries[l] = filename;
      onUsdFilesOpened().emit({l});
      // register_widget(label, ui::build_usd_tree_node(scene->getRootPrim().get()));

      auto rt = scene->getPrim("/");
      // Shared<ZsPrimitive> ret{build_primitive_from_usdprim(rt.get(), 0.)};
      Shared<ZsPrimitive> ret{build_primitive_from_usdprim(rt.get())};
      ret->label() = label;  // overwrite the "/" root label
      register_scene_primitive(g_defaultSceneLabel, label, ret);
      return scene;
    } else {
      instance()._usdEntries[l] = filename;
      onUsdFilesOpened().emit({l});
      // register_widget(label, ui::build_usd_tree_node(scene->getRootPrim().get()));
    }
    return scene;
  }
  SceneDescConcept *ResourceSystem::get_usd(std::string_view label) {
    auto sceneMgr = zs_get_scene_manager(zs_get_world());
    // fmt::print("get usd label [{}] scene addr [{}]\n", label,
    //            (void *)sceneMgr->getScene(label.data()));
    return sceneMgr->getScene(label.data());
  }
  bool ResourceSystem::save_usd_as(std::string_view filename, std::string_view label) {
    (*instance()._usdEntries.find(label)).second = filename;
    auto sceneMgr = zs_get_scene_manager(zs_get_world());

    zs::SceneConfig conf;
    conf.setString("dstPath", filename.data());

    auto scene = sceneMgr->getScene(label.data());
    auto ret = scene->exportScene(conf);
    if (ret) instance()._usdEntries[scene->getName()].markClean();
    return ret;
  }
  bool ResourceSystem::save_usd(std::string_view label) {
    if (auto it = instance()._usdEntries.find(label); it != instance()._usdEntries.end()) {
      auto sceneMgr = zs_get_scene_manager(zs_get_world());

      zs::SceneConfig conf;
      conf.setString("dstPath", (*it).second.getFileName().data());

      auto scene = sceneMgr->getScene(label.data());
      auto ret = scene->exportScene(conf);
      if (ret) instance()._usdEntries[scene->getName()].markClean();
      return ret;
    }
    return false;
  }

  bool ResourceSystem::close_usd(const std::string &label) {
    [[maybe_unused]] bool deleted = instance()._usdEntries.erase(label);

    auto sceneMgr = zs_get_scene_manager(zs_get_world());
    auto ret = sceneMgr->removeScene(label.data());
    // assert((deleted == ret) && "usd file existence divergence happened upon closing!");
    onUsdFilesClosed().emit({label});
    return ret;
  }
#endif

}  // namespace zs