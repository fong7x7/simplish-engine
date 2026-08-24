#pragma once

namespace eng {

/// Tracks which Xbox DX12 resources have been created. Actual D3D12
/// pointers are stored internally in the .cpp file.
struct XboxDx12Resources {
  /// True if the D3D12 device has been created successfully.
  bool device_valid = false;
  /// True if the swap chain has been created successfully.
  bool swap_chain_valid = false;
  /// True if DirectStorage has been initialised successfully.
  bool direct_storage_valid = false;
};

}  // namespace eng
