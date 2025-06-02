add_rules("mode.debug", "mode.release")

add_repositories("liteldev-repo https://github.com/LiteLDev/xmake-repo.git")
add_repositories("groupmountain-repo https://github.com/GroupMountain/xmake-repo.git")

add_requires("levilamina 1.2.0", {configs = {target_type = "server"}})
add_requires("levibuildscript 0.4.0")
add_requires("gmlib 1.2.0-rc.1")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("DebugStick")
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")
    add_cxflags(
        "/EHa",
        "/utf-8",
        "/W4",
        "/w44265",
        "/w44289",
        "/w44296",
        "/w45263",
        "/w44738",
        "/w45204"
    )
    add_defines(
        "NOMINMAX",
        "UNICODE",
        "_HAS_CXX23=1"
    )
    add_packages(
        "levilamina",
        "gmlib"
    )
    set_exceptions("none")
    set_kind("shared")
    set_languages("cxx20")
    set_symbols("debug")
    add_headerfiles("src/**.h")
    add_files("src/**.cpp")
    add_includedirs("src")
    set_optimize("fastest")
    after_build(function (target) 
        local lang_path = path.join(os.projectdir(), "bin", target:name(), "lang")
        os.rm(lang_path)
        os.cp(path.join(os.projectdir(), "lang"), lang_path)
    end)