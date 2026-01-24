"""Set of macros used to help define protobuf library
"""

load("@rules_proto_grpc_cpp//:defs.bzl", "cpp_proto_library")
load("@rules_proto_grpc_python//:defs.bzl", "python_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

_PROTOBUF_DEPENDECIES = [
    "@protobuf//:any_proto",
    "@protobuf//:duration_proto",
    "@protobuf//:empty_proto",
    "@protobuf//:struct_proto",
    "@protobuf//:timestamp_proto",
    "@protobuf//:wrappers_proto",
]

_PROTOBUF_CC_DEPENDECIES = [
    "@protobuf//:any_cc_proto",
    "@protobuf//:duration_cc_proto",
    "@protobuf//:empty_cc_proto",
    "@protobuf//:struct_cc_proto",
    "@protobuf//:timestamp_cc_proto",
    "@protobuf//:wrappers_cc_proto",
]

def api_proto_package(
        name = "pkg",
        srcs = [],
        deps = [],
        visibility = ["//visibility:public"]):
    """Function description.

    Args:
        name: Target name
        srcs: Explicit list of protobuf source files
        deps: Other protobuf library dependecies
        visibility: Rule visibility set to public by default
    """
    if srcs == []:
        srcs = native.glob(["*.proto"])
    relative_name = ":" + name

    proto_library(
        name = name,
        srcs = srcs,
        deps = deps + _PROTOBUF_DEPENDECIES,
        visibility = visibility,
    )

    cpp_proto_library(
        name = name + "_cc_proto",
        protos = [relative_name],
        deps = _PROTOBUF_CC_DEPENDECIES,
        visibility = visibility,
    )

    python_proto_library(
        name = name + "_py_proto",
        protos = [relative_name],
        deps = _PROTOBUF_CC_DEPENDECIES,
        visibility = visibility,
    )
