from conan import ConanFile
from conan.tools.files import get, replace_in_file, copy

# Patch Rapidjson/1.1.0:
# - fix 'error: assignment of read-only member 'rapidjson::GenericStringRef<CharType>::length'' for GCC >=14
#
# Register recipe:
# conan export . --user local --channel patched
#
# In conanfile.txt use:
# rapidjson/1.1.0@local/patched
#

class RapidjsonPatchedConan(ConanFile):
    name = "rapidjson"
    version = "1.1.0"
    package_type = "header-library"

    settings = "os", "arch", "compiler", "build_type"
    no_copy_source = True

    def layout(self):
        self.folders.source = "src"

    def source(self):
        get(
            self,
            url="https://github.com/Tencent/rapidjson/archive/refs/tags/v1.1.0.tar.gz",
            strip_root=True
        )

        replace_in_file(
            self,
            file_path=self.source_folder + "/include/rapidjson/document.h",
            search="GenericStringRef& operator=(const GenericStringRef& rhs) { s = rhs.s; length = rhs.length; }",
            replace="GenericStringRef& operator=(const GenericStringRef& rhs) = delete;"
        )

    def package(self):
        copy(self, "*.h",
             src=self.source_folder + "/include",
             dst=self.package_folder + "/include")

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.set_property("cmake_file_name", "rapidjson")
        self.cpp_info.set_property("cmake_target_name", "rapidjson")
