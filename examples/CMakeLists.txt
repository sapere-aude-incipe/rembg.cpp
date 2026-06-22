add_executable(rembg_mask rembg_mask.cpp)
target_link_libraries(rembg_mask PRIVATE rembg::rembg)
rembg_copy_onnxruntime(rembg_mask)

add_executable(rembg_bytes rembg_bytes.cpp)
target_link_libraries(rembg_bytes PRIVATE rembg::rembg)
rembg_copy_onnxruntime(rembg_bytes)
