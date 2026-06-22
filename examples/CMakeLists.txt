add_executable(rembg_mask rembg_mask.cpp)
target_link_libraries(rembg_mask PRIVATE rembg::rembg)
rembg_copy_onnxruntime(rembg_mask)
