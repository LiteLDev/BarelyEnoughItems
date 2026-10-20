add_rules("mode.debug", "mode.release")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

set_policy("package.requires_lock", true)

-- BEI is a client-only mod. The option exists because the levilamina package and
-- levibuildscript rules read it.
option("target_type")
    set_default("client")
    set_showmenu(true)
    set_values("client")
option_end()

option("tests")
    set_default(true)
    set_showmenu(true)
    set_description("Build the host-side unit test binary (bei-tests)")
option_end()

add_requires("levilamina 26.51.7", {configs = {target_type = "client"}})
add_requires("levibuildscript")
add_requires("nlohmann_json v3.12.0")
if has_config("tests") then
    add_requires("gtest")
end

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

local function bei_common_flags()
    set_languages("c++20")
    set_toolchains("clang-cl")
    add_defines("NOMINMAX", "UNICODE", "_HAS_CXX23=1")
    add_cxflags("/utf-8", "/W4")
    add_cxflags(
        "-Wno-microsoft-cast",
        "-Wno-invalid-offsetof",
        "-Wno-c++2b-extensions",
        "-Wno-microsoft-include",
        "-Wno-overloaded-virtual",
        "-Wno-ignored-qualifiers",
        "-Wno-missing-field-initializers",
        "-Wno-potentially-evaluated-expression",
        "-Wno-pragma-system-header-outside-header",
        {tools = {"clang_cl"}}
    )
end

-- Pure, game-independent logic (src/core). No mc/ll headers allowed here.
target("bei-core")
    set_kind("static")
    bei_common_flags()
    set_exceptions("none")
    add_cxflags("/EHa")
    add_packages("nlohmann_json", {public = true})
    add_includedirs("src", {public = true})
    add_headerfiles("src/core/**.h")
    add_files("src/core/**.cpp")

-- The mod itself.
target("BEI")
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")
    set_kind("shared")
    bei_common_flags()
    set_exceptions("none") -- To avoid conflicts with /EHa.
    add_cxflags("/EHa", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
    add_cxflags("/EHs", {tools = {"clang_cl"}})
    set_symbols("debug")
    add_deps("bei-core")
    add_packages("levilamina", "nlohmann_json")
    add_includedirs("src")
    add_headerfiles("src/**.h")
    add_files("src/**.cpp|core/**.cpp")
    -- Merge per-unit language fragments (assets/lang/<locale>/*.json) into
    -- bin/BEI/lang/<locale>.json next to the mod.
    after_build(function (target)
        import("core.base.json")
        local outdir = path.join(os.projectdir(), "bin", target:name(), "lang")
        os.mkdir(outdir)
        for _, localedir in ipairs(os.dirs(path.join(os.projectdir(), "assets", "lang", "*"))) do
            local merged = {}
            for _, file in ipairs(os.files(path.join(localedir, "*.json"))) do
                for k, v in pairs(json.loadfile(file)) do
                    if merged[k] ~= nil and merged[k] ~= v then
                        raise("duplicate lang key %s in %s", k, file)
                    end
                    merged[k] = v
                end
            end
            json.savefile(path.join(outdir, path.filename(localedir) .. ".json"), merged)
        end
    end)

if has_config("tests") then
    target("bei-tests")
        set_kind("binary")
        set_default(false)
        bei_common_flags()
        set_exceptions("cxx")
        add_deps("bei-core")
        add_packages("gtest", "nlohmann_json")
        add_files("tests/**.cpp")
        add_tests("default")
end
