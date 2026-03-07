#!/bin/bash
# generate-error-handler.sh - Multi-language error handling pattern generator
# Part of CAROL Framework v1.0.0

set -e

VERSION="1.0.0"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Default values
LANGUAGE=""
ERROR_TYPE=""
FUNCTION_NAME=""
VAR_NAME="value"
RETURN_TYPE=""
OUTPUT_FILE=""

# Supported languages and types
SUPPORTED_LANGUAGES="cpp go python rust typescript"
SUPPORTED_TYPES="null-check bounds-check file-error network-error parse-error"

usage() {
    cat << EOF
generate-error-handler.sh - Multi-language error handling pattern generator

Usage:
  generate-error-handler.sh <language> <type> [options]

Supported Languages:
  cpp, go, python, rust, typescript

Error Types:
  null-check      - Null/nil pointer validation
  bounds-check    - Array/vector bounds validation
  file-error      - File operation error handling
  network-error   - Network operation error handling
  parse-error     - Parsing/deserialization error handling

Options:
  --function <name>    Generate for specific function (adds context comment)
  --return-type <type> Specify return type (language-dependent)
  --var <name>         Variable name to check (default: value)
  --output <file>      Write to file (default: stdout)
  --help               Show this help message

Examples:
  # C++ null check
  generate-error-handler.sh cpp null-check --var user

  # Go bounds check
  generate-error-handler.sh go bounds-check --var items

  # Python file error
  generate-error-handler.sh python file-error --var path

CAROL Framework v$VERSION
EOF
    exit 0
}

error() {
    echo -e "${RED}Error: $1${NC}" >&2
    exit "$2"
}

parse_args() {
    for arg in "$@"; do
        if [ "$arg" = "--help" ] || [ "$arg" = "-h" ]; then
            usage
        fi
    done

    while [ $# -gt 0 ]; do
        case "$1" in
            --function)
                FUNCTION_NAME="$2"
                shift 2
                ;;
            --return-type)
                RETURN_TYPE="$2"
                shift 2
                ;;
            --var)
                VAR_NAME="$2"
                shift 2
                ;;
            --output)
                OUTPUT_FILE="$2"
                shift 2
                ;;
            -*)
                error "Unknown flag: $1" 1
                ;;
            *)
                if [ -z "$LANGUAGE" ]; then
                    LANGUAGE="$1"
                elif [ -z "$ERROR_TYPE" ]; then
                    ERROR_TYPE="$1"
                else
                    error "Too many arguments" 1
                fi
                shift
                ;;
        esac
    done

    if [ -z "$LANGUAGE" ] || [ -z "$ERROR_TYPE" ]; then
        error "Missing required arguments. Use --help for usage." 1
    fi

    # Validate language
    if ! echo "$SUPPORTED_LANGUAGES" | grep -wq "$LANGUAGE"; then
        error "Unsupported language: $LANGUAGE" 1
    fi

    # Validate error type
    if ! echo "$SUPPORTED_TYPES" | grep -wq "$ERROR_TYPE"; then
        error "Unsupported error type: $ERROR_TYPE" 1
    fi
}

# Generate function context comment
gen_comment() {
    if [ -n "$FUNCTION_NAME" ]; then
        case "$LANGUAGE" in
            cpp|rust|typescript|go)
                echo "// Error handling for $FUNCTION_NAME"
                ;;
            python)
                echo "# Error handling for $FUNCTION_NAME"
                ;;
        esac
    fi
}

# C++ error handlers
gen_cpp_null_check() {
    gen_comment
    cat << EOF
if (!${VAR_NAME}) {
    throw std::invalid_argument("${VAR_NAME} cannot be null");
}
EOF
}

gen_cpp_bounds_check() {
    gen_comment
    cat << EOF
if (index < 0 || index >= static_cast<int>(${VAR_NAME}.size())) {
    throw std::out_of_range("index out of bounds: " + std::to_string(index));
}
EOF
}

gen_cpp_file_error() {
    gen_comment
    cat << EOF
std::ifstream file(${VAR_NAME});
if (!file.is_open()) {
    throw std::runtime_error("Failed to open file: " + ${VAR_NAME});
}
EOF
}

gen_cpp_network_error() {
    gen_comment
    cat << EOF
if (response.status_code != 200) {
    throw std::runtime_error("Network request failed with status: " +
                            std::to_string(response.status_code));
}
EOF
}

gen_cpp_parse_error() {
    gen_comment
    cat << EOF
try {
    auto parsed = json::parse(${VAR_NAME});
} catch (const json::parse_error& e) {
    throw std::runtime_error("JSON parse error: " + std::string(e.what()));
}
EOF
}

# Go error handlers
gen_go_null_check() {
    gen_comment
    cat << EOF
if ${VAR_NAME} == nil {
    return fmt.Errorf("${VAR_NAME} cannot be nil")
}
EOF
}

gen_go_bounds_check() {
    gen_comment
    cat << EOF
if index < 0 || index >= len(${VAR_NAME}) {
    return fmt.Errorf("index out of bounds: %d", index)
}
EOF
}

gen_go_file_error() {
    gen_comment
    cat << EOF
file, err := os.Open(${VAR_NAME})
if err != nil {
    return fmt.Errorf("failed to open file %s: %w", ${VAR_NAME}, err)
}
defer file.Close()
EOF
}

gen_go_network_error() {
    gen_comment
    cat << EOF
if response.StatusCode != http.StatusOK {
    return fmt.Errorf("network request failed with status: %d", response.StatusCode)
}
EOF
}

gen_go_parse_error() {
    gen_comment
    cat << EOF
var result map[string]interface{}
if err := json.Unmarshal([]byte(${VAR_NAME}), &result); err != nil {
    return fmt.Errorf("JSON parse error: %w", err)
}
EOF
}

# Python error handlers
gen_python_null_check() {
    gen_comment
    cat << EOF
if ${VAR_NAME} is None:
    raise ValueError("${VAR_NAME} cannot be None")
EOF
}

gen_python_bounds_check() {
    gen_comment
    cat << EOF
if not (0 <= index < len(${VAR_NAME})):
    raise IndexError(f"index out of bounds: {index}")
EOF
}

gen_python_file_error() {
    gen_comment
    cat << EOF
try:
    with open(${VAR_NAME}, 'r') as f:
        content = f.read()
except FileNotFoundError:
    raise FileNotFoundError(f"File not found: {${VAR_NAME}}")
except IOError as e:
    raise IOError(f"Failed to read file {${VAR_NAME}}: {e}")
EOF
}

gen_python_network_error() {
    gen_comment
    cat << EOF
if response.status_code != 200:
    raise RuntimeError(f"Network request failed with status: {response.status_code}")
EOF
}

gen_python_parse_error() {
    gen_comment
    cat << EOF
try:
    parsed = json.loads(${VAR_NAME})
except json.JSONDecodeError as e:
    raise ValueError(f"JSON parse error: {e}")
EOF
}

# Rust error handlers
gen_rust_null_check() {
    gen_comment
    cat << EOF
if ${VAR_NAME}.is_none() {
    return Err("${VAR_NAME} cannot be None".to_string());
}
EOF
}

gen_rust_bounds_check() {
    gen_comment
    cat << EOF
if index >= ${VAR_NAME}.len() {
    return Err(format!("index out of bounds: {}", index));
}
EOF
}

gen_rust_file_error() {
    gen_comment
    cat << EOF
let content = std::fs::read_to_string(${VAR_NAME})
    .map_err(|e| format!("Failed to read file {}: {}", ${VAR_NAME}, e))?;
EOF
}

gen_rust_network_error() {
    gen_comment
    cat << EOF
match response {
    Ok(data) => data,
    Err(e) => return Err(format!("Network request failed: {}", e)),
}
EOF
}

gen_rust_parse_error() {
    gen_comment
    cat << EOF
let parsed: serde_json::Value = serde_json::from_str(${VAR_NAME})
    .map_err(|e| format!("JSON parse error: {}", e))?;
EOF
}

# TypeScript error handlers
gen_typescript_null_check() {
    gen_comment
    cat << EOF
if (${VAR_NAME} === null || ${VAR_NAME} === undefined) {
    throw new Error("${VAR_NAME} cannot be null or undefined");
}
EOF
}

gen_typescript_bounds_check() {
    gen_comment
    cat << EOF
if (index < 0 || index >= ${VAR_NAME}.length) {
    throw new RangeError(\`index out of bounds: \${index}\`);
}
EOF
}

gen_typescript_file_error() {
    gen_comment
    cat << EOF
try {
    const content = await fs.promises.readFile(${VAR_NAME}, 'utf-8');
} catch (error) {
    throw new Error(\`Failed to read file \${${VAR_NAME}}: \${error}\`);
}
EOF
}

gen_typescript_network_error() {
    gen_comment
    cat << EOF
if (response.status !== 200) {
    throw new Error(\`Network request failed with status: \${response.status}\`);
}
EOF
}

gen_typescript_parse_error() {
    gen_comment
    cat << EOF
try {
    const parsed = JSON.parse(${VAR_NAME});
} catch (error) {
    throw new Error(\`JSON parse error: \${error}\`);
}
EOF
}

# Generate appropriate error handler
generate_handler() {
    local func_name="gen_${LANGUAGE}_${ERROR_TYPE}"
    func_name=$(echo "$func_name" | tr '-' '_')

    if type "$func_name" > /dev/null 2>&1; then
        $func_name
    else
        error "Handler not implemented: $LANGUAGE $ERROR_TYPE" 1
    fi
}

# Main
main() {
    parse_args "$@"

    local output=$(generate_handler)

    if [ -n "$OUTPUT_FILE" ]; then
        echo "$output" > "$OUTPUT_FILE"
        echo -e "${GREEN}✓${NC} Generated $LANGUAGE $ERROR_TYPE handler: $OUTPUT_FILE" >&2
    else
        echo "$output"
    fi

    exit 0
}

main "$@"
