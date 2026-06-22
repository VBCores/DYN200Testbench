#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT_C_DIR="${ROOT_DIR}/generated/c"
OUT_CPP_DIR="${ROOT_DIR}/generated/cpp"
LIBCXXCANARD_DIR="${LIBCXXCANARD_DIR:-}"
if [[ -z "${LIBCXXCANARD_DIR}" && -d "${ROOT_DIR}/build/_deps/libcxxcanard-src" ]]; then
    LIBCXXCANARD_DIR="${ROOT_DIR}/build/_deps/libcxxcanard-src"
fi
TRAITS_DIR="${LIBCXXCANARD_DIR}/nunavut_templates/cxxcanard_traits"
mkdir -p "${OUT_C_DIR}" "${OUT_CPP_DIR}"

NNVG_BIN="${NNVG_EXECUTABLE:-}"
if [[ -z "${NNVG_BIN}" ]]; then
    if command -v nnvg >/dev/null 2>&1; then
        NNVG_BIN="$(command -v nnvg)"
    elif [[ -x "${ROOT_DIR}/../.venv/bin/nnvg" ]]; then
        NNVG_BIN="${ROOT_DIR}/../.venv/bin/nnvg"
    elif [[ -x "${ROOT_DIR}/.venv/bin/nnvg" ]]; then
        NNVG_BIN="${ROOT_DIR}/.venv/bin/nnvg"
    fi
fi

if [[ -z "${NNVG_BIN}" || ! -x "${NNVG_BIN}" ]]; then
    echo "nnvg was not found. Install Nunavut or set NNVG_EXECUTABLE=/path/to/nnvg." >&2
    exit 1
fi
if [[ ! -d "${TRAITS_DIR}" ]]; then
    echo "libcxxcanard traits templates were not found: ${TRAITS_DIR}" >&2
    echo "Set LIBCXXCANARD_DIR=/path/to/libcxxcanard." >&2
    exit 1
fi

MERGED_DIR="$(mktemp -d)"
trap 'rm -rf "${MERGED_DIR}"' EXIT
mkdir -p "${MERGED_DIR}/voltbro"
cp -R "${ROOT_DIR}/dsdl/voltbro_types/voltbro/." "${MERGED_DIR}/voltbro/"
cp -R "${ROOT_DIR}/dsdl/project_types/voltbro/." "${MERGED_DIR}/voltbro/"

"${NNVG_BIN}" \
  --target-language c \
  --language-standard c11 \
  --outdir "${OUT_C_DIR}" \
  --lookup-dir "${ROOT_DIR}/dsdl/public_regulated_data_types/uavcan" \
  --lookup-dir "${ROOT_DIR}/dsdl/public_regulated_data_types/reg" \
  "${MERGED_DIR}/voltbro"

"${NNVG_BIN}" \
  --target-language c \
  --language-standard c11 \
  --templates "${TRAITS_DIR}" \
  --output-extension hpp \
  --generate-support never \
  --outdir "${OUT_CPP_DIR}" \
  --lookup-dir "${ROOT_DIR}/dsdl/public_regulated_data_types/uavcan" \
  --lookup-dir "${ROOT_DIR}/dsdl/public_regulated_data_types/reg" \
  "${MERGED_DIR}/voltbro"

"${NNVG_BIN}" \
  --target-language c \
  --language-standard c11 \
  --templates "${TRAITS_DIR}" \
  --output-extension hpp \
  --generate-support never \
  --outdir "${OUT_CPP_DIR}" \
  --lookup-dir "${ROOT_DIR}/dsdl/public_regulated_data_types/reg" \
  "${ROOT_DIR}/dsdl/public_regulated_data_types/uavcan"

"${NNVG_BIN}" \
  --target-language c \
  --language-standard c11 \
  --templates "${TRAITS_DIR}" \
  --output-extension hpp \
  --generate-support never \
  --outdir "${OUT_CPP_DIR}" \
  --lookup-dir "${ROOT_DIR}/dsdl/public_regulated_data_types/uavcan" \
  "${ROOT_DIR}/dsdl/public_regulated_data_types/reg"
