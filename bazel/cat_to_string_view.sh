#!/bin/bash

cat "$1" << EOF
constexpr std::string_view MY_STRING_VIEW = R"(
$(cat "$1")
)";
EOF