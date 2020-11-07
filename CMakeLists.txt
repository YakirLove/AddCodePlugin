set(LLVM_LINK_COMPONENTS support)

add_clang_executable(AddCodePlugin
  AddCodePlugin.cpp
)

target_link_libraries(AddCodePlugin
    PRIVATE
    clangAST
    clangBasic
    clangDriver
    clangFormat
    clangLex
    clangParse
    clangSema
    clangFrontend
    clangTooling
    clangToolingCore
    clangRewrite
    clangRewriteFrontend
)
