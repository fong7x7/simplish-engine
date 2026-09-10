#include "gltf-rig-builder.h"

#include "gltf-accessor.h"
#include "gltf-node-pose.h"

#include <algorithm>
#include <cstring>
#include <engine/render-mesh/skin-palette.h>
#include <string>

namespace eng::gltf {

namespace {

  /// A node with no parent.
  constexpr uint32_t NO_NODE = std::numeric_limits<uint32_t>::max();

  /// Each node's parent, or `NO_NODE`. Nullopt when a node is listed as a
  /// child twice or of itself, or a child names a node that is not there —
  /// a hierarchy that is not a forest.
  std::optional<std::vector<uint32_t>> nodeParents(const Json& root) {
    const size_t count = jsonArraySize(root, "nodes");
    std::vector<uint32_t> parents(count, NO_NODE);
    for (size_t n = 0; n < count; ++n) {
      const Json& node = *jsonElement(root, "nodes", n);
      for (size_t c = 0; c < jsonArraySize(node, "children"); ++c) {
        const auto child = jsonIndexAt(node, "children", c);
        if (!child || *child >= count || *child == n ||
            parents[*child] != NO_NODE) {
          return std::nullopt;
        }
        parents[*child] = static_cast<uint32_t>(n);
      }
    }
    return parents;
  }

  /// The node numbers skin @p skin lists as its joints, in its order.
  /// Nullopt when one is not a node.
  std::optional<std::vector<uint32_t>> skinJointNodes(const Json& skin,
                                                      size_t node_count) {
    std::vector<uint32_t> joints;
    for (size_t k = 0; k < jsonArraySize(skin, "joints"); ++k) {
      const auto node = jsonIndexAt(skin, "joints", k);
      if (!node || *node >= node_count) {
        return std::nullopt;
      }
      joints.push_back(static_cast<uint32_t>(*node));
    }
    return joints;
  }

  /// Flag every joint and every node above one.
  std::vector<uint8_t> neededNodes(const std::vector<uint32_t>& parents,
                                   const std::vector<uint32_t>& joints) {
    std::vector<uint8_t> needed(parents.size(), 0);
    for (uint32_t node : joints) {
      // Stops at a node already flagged, so a loop in the hierarchy ends
      // the walk rather than spinning; `skeletonOrder` then misses the
      // loop's nodes, which is how a loop is detected.
      while (node != NO_NODE && needed[node] == 0) {
        needed[node] = 1;
        node = parents[node];
      }
    }
    return needed;
  }

  /// Push @p node's needed children onto @p stack, last first, so they pop
  /// in the order the file lists them.
  void pushChildren(const Json& node, const std::vector<uint8_t>& needed,
                    std::vector<uint32_t>& stack) {
    for (size_t c = jsonArraySize(node, "children"); c > 0; --c) {
      const auto child = jsonIndexAt(node, "children", c - 1);
      if (child && needed[*child] != 0) {
        stack.push_back(static_cast<uint32_t>(*child));
      }
    }
  }

  /// The needed nodes depth-first from each root in turn: every parent
  /// before its children, siblings in the file's order.
  std::vector<uint32_t> skeletonOrder(const Json& root,
                                      const std::vector<uint32_t>& parents,
                                      const std::vector<uint8_t>& needed) {
    std::vector<uint32_t> order;
    std::vector<uint32_t> stack;
    for (uint32_t n = 0; n < parents.size(); ++n) {
      if (needed[n] != 0 && parents[n] == NO_NODE) {
        stack.push_back(n);
      }
      while (!stack.empty()) {
        const uint32_t node = stack.back();
        stack.pop_back();
        order.push_back(node);
        pushChildren(*jsonElement(root, "nodes", node), needed, stack);
      }
    }
    return order;
  }

  /// A node's name, or one made from its number when it has none.
  std::string nodeName(const Json& node, uint32_t index) {
    std::string name = jsonString(node, "name");
    return name.empty() ? "node " + std::to_string(index) : name;
  }

  /// The skeleton for @p order, and the node-to-joint map.
  GltfRig skeletonFrom(const Json& root, const std::vector<uint32_t>& parents,
                       const std::vector<uint32_t>& order) {
    GltfRig rig;
    rig.joint_of_node.assign(parents.size(), GLTF_NOT_A_JOINT);
    for (size_t j = 0; j < order.size(); ++j) {
      rig.joint_of_node[order[j]] = static_cast<uint32_t>(j);
    }
    for (const uint32_t node : order) {
      const Json& entry = *jsonElement(root, "nodes", node);
      const uint32_t parent = parents[node];
      rig.skeleton.parents.push_back(parent == NO_NODE
                                         ? animation::SKELETON_NO_PARENT
                                         : rig.joint_of_node[parent]);
      rig.skeleton.rest.push_back(gltfNodePose(entry));
      rig.skeleton.names.push_back(nodeName(entry, node));
    }
    return rig;
  }

  /// The skin's inverse bind matrices, or identities when it gives none —
  /// which glTF defines as meaning the joints were bound where they stand.
  std::optional<std::vector<Mat4>>
  inverseBinds(const GltfDocument& document, const Json& skin, size_t joints) {
    const auto accessor = jsonIndex(skin, "inverseBindMatrices");
    if (!accessor) {
      return std::vector<Mat4>(joints, Mat4::identity());
    }
    const auto floats = readAccessorFloats(document, *accessor, 16);
    if (!floats || floats->size() < joints * 16) {
      return std::nullopt;
    }
    std::vector<Mat4> matrices(joints);
    for (size_t k = 0; k < joints; ++k) {
      std::memcpy(matrices[k].m, floats->data() + k * 16, sizeof(Mat4::m));
    }
    return matrices;
  }

  /// The rig for a skin whose joints and hierarchy have been checked.
  std::expected<GltfRig, GltfLoadError>
  rigFromJoints(const GltfDocument& document, const Json& skin,
                const std::vector<uint32_t>& parents,
                const std::vector<uint32_t>& joints) {
    const auto needed = neededNodes(parents, joints);
    const auto order = skeletonOrder(document.root, parents, needed);
    if (order.size() != static_cast<size_t>(std::ranges::count(needed, 1))) {
      return std::unexpected(GltfLoadError::BAD_SKIN);
    }
    GltfRig rig = skeletonFrom(document.root, parents, order);
    auto binds = inverseBinds(document, skin, joints.size());
    if (!binds) {
      return std::unexpected(GltfLoadError::BAD_ACCESSOR);
    }
    for (const uint32_t node : joints) {
      rig.skin.joints.push_back(rig.joint_of_node[node]);
    }
    rig.skin.inverse_bind = std::move(*binds);
    return rig;
  }

}  // namespace

std::expected<GltfRig, GltfLoadError> buildGltfRig(const GltfDocument& document,
                                                   size_t skin) {
  const Json* entry = jsonElement(document.root, "skins", skin);
  const auto parents = nodeParents(document.root);
  if (entry == nullptr || !parents) {
    return std::unexpected(GltfLoadError::BAD_SKIN);
  }
  const auto joints = skinJointNodes(*entry, parents->size());
  if (!joints || joints->empty()) {
    return std::unexpected(GltfLoadError::BAD_SKIN);
  }
  if (joints->size() > MESH_MAX_SKIN_JOINTS) {
    return std::unexpected(GltfLoadError::TOO_MANY_JOINTS);
  }
  return rigFromJoints(document, *entry, *parents, *joints);
}

}  // namespace eng::gltf
