add_executable(LWGFAssetPacker IMPORTED GLOBAL)
set_target_properties(LWGFAssetPacker PROPERTIES IMPORTED_LOCATION "${PACKER}")
include("${ASSET_MODULE}")
