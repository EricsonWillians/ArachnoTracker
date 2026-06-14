#!/usr/bin/env bash
set -euo pipefail

#############################################
# ArachnoTracker Fancy Build Script
#############################################

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
SOURCE_DIR="${ROOT_DIR}"
THREADS="${JOBS:-$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 8)}"
BUILD_TYPE="Release"
RECONFIGURE=0
CLEAN=0
RUN_TESTS=0
RUN_APP=0
APP_ARGS=()

echo_banner() {
    printf "\n"
    printf "\033[1;38;5;208m╔══════════════════════════════════════════════════════════════════════╗\033[0m\n"
    printf "\033[1;38;5;208m║         ArachnoTracker — Fancy Build Orchestrator                  ║\033[0m\n"
    printf "\033[1;38;5;208m╚══════════════════════════════════════════════════════════════════════╝\033[0m\n"
    printf "\n"
}

log_step() {
    local color="\033[1;36m"
    printf "\n${color}==>%s\033[0m\n" "$1"
}

log_ok() {
    printf "\033[1;32m[OK]\033[0m %s\n" "$1"
}

log_warn() {
    printf "\033[1;33m[WARN]\033[0m %s\n" "$1"
}

log_err() {
    printf "\033[1;31m[ERR]\033[0m %s\n" "$1" >&2
}

require() {
    if ! command -v "$1" >/dev/null 2>&1; then
        log_err "Missing required command: $1"
        exit 1
    fi
}

usage() {
    cat <<'USAGE'
Usage:
  ./build.sh [options]

Options:
  -h, --help             Show this help.
  -c, --clean            Remove and recreate build directory before configuring.
  -r, --reconfigure      Re-run cmake configure step.
  -t, --type <Debug|Release>
                         Choose build type (default: Release).
  -j, --jobs <N>         Override parallel jobs (default: auto).
      --ccache           Enable ccache if available.
      --run               Run ArachnoTracker after successful build.
      --test              Run ctest after build.
      -- <args>           Additional args forwarded to ArachnoTracker after --run.

Examples:
  ./build.sh
  ./build.sh --type Debug --jobs 12
  ./build.sh --clean --reconfigure --test
  ./build.sh --run -- --gui example.arachno
USAGE
}

parse_args() {
    local positional=()
    while [[ $# -gt 0 ]]; do
        case "$1" in
            -h|--help)
                usage
                exit 0
                ;;
            -c|--clean)
                CLEAN=1
                shift
                ;;
            -r|--reconfigure)
                RECONFIGURE=1
                shift
                ;;
            -t|--type)
                if [[ $# -lt 2 ]]; then
                    log_err "--type requires Debug or Release"
                    exit 1
                fi
                BUILD_TYPE="$2"
                shift 2
                ;;
            -j|--jobs)
                if [[ $# -lt 2 ]]; then
                    log_err "--jobs requires a number"
                    exit 1
                fi
                THREADS="$2"
                shift 2
                ;;
            --ccache)
                CMAKE_CMAKE_OPTS+=("-DCMAKE_CXX_COMPILER_LAUNCHER=ccache")
                CMAKE_CMAKE_OPTS+=("-DCMAKE_C_COMPILER_LAUNCHER=ccache")
                shift
                ;;
            --run)
                RUN_APP=1
                shift
                ;;
            --test)
                RUN_TESTS=1
                shift
                ;;
            --)
                shift
                APP_ARGS+=("$@")
                break
                ;;
            *)
                positional+=("$1")
                shift
                ;;
        esac
    done

    if [[ ${#positional[@]} -ne 0 ]]; then
        log_warn "Unknown args: ${positional[*]}"
    fi
}

run_step() {
    local msg="$1"
    local cmd="$2"
    log_step "$msg"
    eval "$cmd"
    log_ok "$msg"
}

require cmake
require ninja || require make
require c++

CMAKE_CMAKE_OPTS=()
parse_args "$@"

if [[ "$BUILD_TYPE" != "Debug" && "$BUILD_TYPE" != "Release" && "$BUILD_TYPE" != "RelWithDebInfo" ]]; then
    log_err "Invalid build type: $BUILD_TYPE (expected Debug|Release|RelWithDebInfo)"
    exit 1
fi

echo_banner

if [[ "$CLEAN" -eq 1 ]]; then
    log_step "Cleaning build directory"
    rm -rf "${BUILD_DIR}"
    log_ok "Clean complete"
fi

if [[ ! -d "${BUILD_DIR}" ]]; then
    RECONFIGURE=1
fi

if [[ "$RECONFIGURE" -eq 1 ]]; then
    run_step "Configuring (${BUILD_TYPE})" "cmake -S \"${SOURCE_DIR}\" -B \"${BUILD_DIR}\" -G Ninja -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ${CMAKE_CMAKE_OPTS[*]}"
else
    run_step "Regenerating build files" "cmake -S \"${SOURCE_DIR}\" -B \"${BUILD_DIR}\""
fi

run_step "Building" "cmake --build \"${BUILD_DIR}\" --parallel ${THREADS}"

if [[ "$RUN_TESTS" -eq 1 ]]; then
    run_step "Running CTest" "ctest --test-dir \"${BUILD_DIR}\" --output-on-failure"
fi

if [[ "$RUN_APP" -eq 1 ]]; then
    run_step "Launching application" "${BUILD_DIR}/ArachnoTracker \"${APP_ARGS[@]}\""
fi

printf "\n\033[1;32mBuild finished successfully.\033[0m\n"
printf "Binary: \033[1m%s\033[0m\n" "${BUILD_DIR}/ArachnoTracker" 
