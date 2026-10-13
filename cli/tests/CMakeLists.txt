# tests — bộ kiểm thử BYTE-EXACT (xem AGENTS.md §0).
#
# Nguyên tắc: mỗi thuật toán được port phải khớp TỪNG BYTE với dữ liệu chuẩn
# sinh từ bản Python gốc. Vector nằm trong tests/vectors/ (sinh lại bằng các
# script gen_vectors*.py), fixture không chứa bí mật nằm trong tests/fixtures/.
#
# Thư mục test phản chiếu cấu trúc của libs/core để biết ngay module nào đang
# được kiểm tra.

# Khai báo một test: tên file (không đuôi) + tên hiển thị trong ctest.
function(tencent_add_test relative_path test_name)
  get_filename_component(target "${relative_path}" NAME_WE)
  add_executable(${target} "${relative_path}.cpp")
  target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(${target} PRIVATE tencent::core tencent::project_options)
  add_test(NAME ${test_name} COMMAND ${target})
  # Fixture được tìm theo đường dẫn tương đối từ thư mục tests/.
  set_tests_properties(${test_name} PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
endfunction()

tencent_add_test(crypto/test_crypto              crypto_byte_exact)
tencent_add_test(codec/test_protobuf             protobuf_byte_exact)
tencent_add_test(net/test_http_client            http_client_smoke)
tencent_add_test(model/test_account              account_parse)
tencent_add_test(model/test_turing_material      turing_material_byte_exact)
tencent_add_test(protocol/test_trpc_head         trpc_head_byte_exact)
tencent_add_test(protocol/test_login_sign        login_sign_byte_exact)
tencent_add_test(protocol/test_vip_info          vip_info_byte_exact)
tencent_add_test(protocol/test_user_center       user_center_parse)
tencent_add_test(protocol/test_capability        capability_byte_exact)
tencent_add_test(protocol/test_request_builder   request_builder_byte_exact)
tencent_add_test(protocol/test_qimei_client      qimei_client_byte_exact)
tencent_add_test(protocol/test_guid_client       guid_client_byte_exact)
tencent_add_test(protocol/test_search_api        search_api_byte_exact)
tencent_add_test(download/test_pipeline          download_pipeline_cache)
tencent_add_test(download/test_proxy_config      download_proxy_config_parse)
