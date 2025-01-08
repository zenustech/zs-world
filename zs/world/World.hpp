#pragma once
#include "world/WorldExport.hpp"
#include "interface/world/NodeInterface.hpp"

#ifdef ZS_ENABLE_USD
#include "usd/USDScenePrim.hpp"
#else
#include "SceneInterface.hpp"
#endif

namespace zs {

struct World;

} // namespace zs

extern "C" {
ZS_WORLD_EXPORT void zs_setup_world();
ZS_WORLD_EXPORT void *zs_world_get_module(const char *path);
ZS_WORLD_EXPORT zs::World *zs_get_world();
///
ZS_WORLD_EXPORT zs::NodeManagerConcept *zs_get_plugin_manager(zs::World *world);
ZS_WORLD_EXPORT zs::SceneManagerConcept* zs_get_scene_manager(zs::World *world);

ZS_WORLD_EXPORT bool
zs_register_node_factory_lite(zs::World *world, const char *label,
                              zs::funcsig_create_node *nodeFactory);
ZS_WORLD_EXPORT bool
zs_register_node_factory(zs::World *world, const char *label,
                         zs::funcsig_create_node *nodeFactory,
                         zs::NodeDescriptorPort descr);
///
ZS_WORLD_EXPORT bool zs_load_plugins(const char *path);
ZS_WORLD_EXPORT void zs_display_nodes();
ZS_WORLD_EXPORT bool zs_execute_node(const char *label);
}
