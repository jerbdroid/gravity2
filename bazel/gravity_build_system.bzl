load("@com_github_google_flatbuffers//:build_defs.bzl", "flatbuffer_cc_library", "flatbuffer_library_public")
load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_python//python:defs.bzl", "py_library")

_GRAVITY_COMPILATION_DEFINE = {
    "opt": ["NDEBUG"],
    "dbg": [],
    "fast": [],
}

_GRAVITY_COPTS = {
    "opt": ["-Wno-nullability-completeness", "-Wno-builtin-macro-redefined", "-Wno-macro-redefined"],
    "dbg": ["-Wno-nullability-completeness", "-Wno-builtin-macro-redefined", "-Wno-macro-redefined"],
    "fast": ["-Wno-nullability-completeness", "-Wno-builtin-macro-redefined", "-Wno-macro-redefined"],
}

def calculate_gravity_defines(defines = []):
    return select({
        "//bazel:opt_build": _GRAVITY_COMPILATION_DEFINE["opt"],
        "//bazel:dbg_build": _GRAVITY_COMPILATION_DEFINE["dbg"],
        "//bazel:fast_build": _GRAVITY_COMPILATION_DEFINE["fast"],
    }) + defines

def calculate_gravity_copts(copts = []):
    return select({
        "//bazel:opt_build": _GRAVITY_COPTS["opt"],
        "//bazel:dbg_build": _GRAVITY_COPTS["dbg"],
        "//bazel:fast_build": _GRAVITY_COPTS["fast"],
    }) + copts

def gravity_cc_library(name, defines = [], deps = [], copts = [], **kargs):
    cc_library(
        name = name,
        defines = calculate_gravity_defines(defines),
        copts = calculate_gravity_copts(copts),
        deps = deps + ["//source/common/diagnostics:trace"],
        **kargs
    )

def gravity_cc_binary(name, defines = [], deps = [], copts = [], **kargs):
    cc_binary(
        name = name,
        deps = deps + ["//source/common/diagnostics:trace"],
        copts = calculate_gravity_copts(copts),
        defines = calculate_gravity_defines(defines),
        **kargs
    )

_GRAVITY_DEFAULT_FLATC_ARGS = [
    "--cpp-ptr-type std::unique_ptr",
    "--cpp-std c++17",
]

_GRAVITY_DEFAULT_FLATC_PY_ARGS = [
    "--gen-onefile",
    "--gen-all",
]

def gravity_flatbuffer_library(name, srcs, outs = [], out_prefix = "", deps = [], visibility = None, **kargs):
    flatbuffer_cc_library(name = name, srcs = srcs, outs = outs, out_prefix = out_prefix, deps = deps, flatc_args = _GRAVITY_DEFAULT_FLATC_ARGS, visibility = visibility, **kargs)

    output = [
        (out_prefix + "%s_generated.py") % (s.replace(".fbs", "").split("/")[-1].split(":")[-1])
        for s in srcs
    ]

    includes = []
    if deps:
        includes = [d + "_py_includes" for d in deps]

    flatbuffer_library_public(
        name = name + "_py_srcs",
        srcs = srcs,
        outs = outs + output,
        language_flag = "--python",
        out_prefix = out_prefix,
        includes = includes,
        visibility = visibility,
        flatc_args = _GRAVITY_DEFAULT_FLATC_PY_ARGS,
    )

    py_library(
        name = name + "_py",
        srcs = [":" + name + "_py_srcs"],
        deps = [x + "_py" for x in deps],
        data = kargs.get("data", []),
        imports = ["."],
        visibility = visibility,
    )
    native.filegroup(
        name = "%s_py_includes" % (name),
        srcs = srcs + includes,
        visibility = visibility,
    )

def gravity_generate_cc_shader_library(name, srcs = [], **kargs):
    native.filegroup(
        name = name + "_generated_shader_source",
        srcs = srcs,
    )

    native.genrule(
        name = name + "_generated_shader",
        srcs = [":" + name + "_generated_shader_source"],
        outs = [name + "_strings.hpp"],
        cmd_bat = "$(location //bazel:cat_to_string_view_bat) $(locations :" + name + "_generated_shader_source) > $@",
        tools = ["//bazel:cat_to_string_view_bat"],
        visibility = ["//visibility:public"],
    )

    gravity_cc_library(
        name = name,
        hdrs = [":" + name + "_generated_shader"],
        includes = ["."],
        **kargs
    )
