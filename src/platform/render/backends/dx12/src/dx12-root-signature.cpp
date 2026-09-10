#include "dx12-root-signature.h"

#ifdef ENGINE_RENDERER_DX12

#include <array>

namespace eng::render {

namespace {

  D3D12_ROOT_PARAMETER makeCbvParam(uint32_t shader_register,
                                    D3D12_SHADER_VISIBILITY visibility) {
    D3D12_ROOT_PARAMETER param{};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    param.Descriptor.ShaderRegister = shader_register;
    param.Descriptor.RegisterSpace = 0;
    param.ShaderVisibility = visibility;
    return param;
  }

  D3D12_ROOT_PARAMETER makeSrvTableParam(const D3D12_DESCRIPTOR_RANGE& range) {
    D3D12_ROOT_PARAMETER param{};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param.DescriptorTable.NumDescriptorRanges = 1;
    param.DescriptorTable.pDescriptorRanges = &range;
    param.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    return param;
  }

  D3D12_DESCRIPTOR_RANGE makeSingleSrvRange() {
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    range.BaseShaderRegister = 0;
    range.RegisterSpace = 0;
    range.OffsetInDescriptorsFromTableStart = 0;
    return range;
  }

  /// A linear sampler with the given addressing, at the given register —
  /// what the Metal shaders declare inline in MSL and HLSL cannot.
  D3D12_STATIC_SAMPLER_DESC
  makeLinearSampler(D3D12_TEXTURE_ADDRESS_MODE address, UINT shader_register) {
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = address;
    sampler.AddressV = address;
    sampler.AddressW = address;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = shader_register;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    return sampler;
  }

  /// Both samplers the built-in pipelines use: `s0` clamps, which is what
  /// the GUI wants for a quad sampled once across its own rect, and `s1`
  /// wraps, which is what a mesh wants so a tiling map tiles. They differ
  /// only in addressing, and the mesh shader picks by register — see
  /// `MESH_HLSL_SOURCE`, whose Metal counterpart declares the same two
  /// modes inline.
  std::array<D3D12_STATIC_SAMPLER_DESC, 2> makeStaticSamplers() {
    return {makeLinearSampler(D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0),
            makeLinearSampler(D3D12_TEXTURE_ADDRESS_MODE_WRAP, 1)};
  }

  void fillGraphicsParams(
      std::array<D3D12_ROOT_PARAMETER, DX12_GRAPHICS_ROOT_PARAM_COUNT>& params,
      const D3D12_DESCRIPTOR_RANGE& srv_range) {
    params[DX12_ROOT_PARAM_VERTEX_CBV0] =
        makeCbvParam(0, D3D12_SHADER_VISIBILITY_VERTEX);
    params[DX12_ROOT_PARAM_VERTEX_CBV1] =
        makeCbvParam(1, D3D12_SHADER_VISIBILITY_VERTEX);
    params[DX12_ROOT_PARAM_PIXEL_CBV0] =
        makeCbvParam(0, D3D12_SHADER_VISIBILITY_PIXEL);
    params[DX12_ROOT_PARAM_PIXEL_CBV1] =
        makeCbvParam(1, D3D12_SHADER_VISIBILITY_PIXEL);
    params[DX12_ROOT_PARAM_PIXEL_SRV_TABLE] = makeSrvTableParam(srv_range);
    params[DX12_ROOT_PARAM_VERTEX_CBV2] =
        makeCbvParam(2, D3D12_SHADER_VISIBILITY_VERTEX);
  }

  /// Serialise and create; releases the blobs either way. Null on failure.
  ID3D12RootSignature* serializeAndCreate(ID3D12Device5* device,
                                          const D3D12_ROOT_SIGNATURE_DESC& d) {
    ID3DBlob* blob = nullptr;
    ID3DBlob* error = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&d, D3D_ROOT_SIGNATURE_VERSION_1,
                                             &blob, &error);
    ID3D12RootSignature* root_sig = nullptr;
    if (SUCCEEDED(hr)) {
      hr = device->CreateRootSignature(0, blob->GetBufferPointer(),
                                       blob->GetBufferSize(),
                                       IID_PPV_ARGS(&root_sig));
      blob->Release();
    }
    if (error != nullptr) {
      error->Release();
    }
    return SUCCEEDED(hr) ? root_sig : nullptr;
  }

}  // namespace

ID3D12RootSignature* createDx12GraphicsRootSignature(ID3D12Device5* device) {
  const D3D12_DESCRIPTOR_RANGE srv_range = makeSingleSrvRange();
  std::array<D3D12_ROOT_PARAMETER, DX12_GRAPHICS_ROOT_PARAM_COUNT> params{};
  fillGraphicsParams(params, srv_range);
  const std::array<D3D12_STATIC_SAMPLER_DESC, 2> samplers =
      makeStaticSamplers();

  D3D12_ROOT_SIGNATURE_DESC desc{};
  desc.NumParameters = DX12_GRAPHICS_ROOT_PARAM_COUNT;
  desc.pParameters = params.data();
  desc.NumStaticSamplers = static_cast<UINT>(samplers.size());
  desc.pStaticSamplers = samplers.data();
  desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
  return serializeAndCreate(device, desc);
}

ID3D12RootSignature* createDx12ComputeRootSignature(ID3D12Device5* device) {
  D3D12_ROOT_SIGNATURE_DESC desc{};
  desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
  return serializeAndCreate(device, desc);
}

}  // namespace eng::render

#endif  // ENGINE_RENDERER_DX12
