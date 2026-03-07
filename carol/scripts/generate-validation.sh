#!/bin/bash
# generate-validation.sh - Input validation code generator
# Part of CAROL Framework v1.0.0

set -e

VERSION="1.0.0"

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

# Default values
LANGUAGE=""
VALIDATION_TYPE=""
VAR_NAME="value"
MIN_VALUE=""
MAX_VALUE=""
ALLOWED_VALUES=""
PREDICATE=""
OUTPUT_FILE=""

SUPPORTED_LANGUAGES="cpp go python rust typescript"
SUPPORTED_TYPES="range string-empty email enum custom"

usage() {
    cat << EOF
generate-validation.sh - Input validation code generator

Usage:
  generate-validation.sh <language> <validation-type> [options]

Validation Types:
  range         - Numeric range check
  string-empty  - Empty string check
  email         - Email format validation
  enum          - Enum/const value validation
  custom        - Custom predicate

Options:
  --var <name>          Variable name
  --min <value>         Minimum value (range)
  --max <value>         Maximum value (range)
  --values <list>       Allowed values (enum, comma-separated)
  --predicate <expr>    Custom validation expression
  --output <file>       Write to file (default: stdout)
  --help                Show this help

Examples:
  # C++ range
  generate-validation.sh cpp range --var volume --min 0.0 --max 1.0

  # Go string empty
  generate-validation.sh go string-empty --var username

  # Python enum
  generate-validation.sh python enum --var status --values "pending,active,done"

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
        [ "$arg" = "--help" ] && usage
    done

    while [ $# -gt 0 ]; do
        case "$1" in
            --var)
                VAR_NAME="$2"
                shift 2
                ;;
            --min)
                MIN_VALUE="$2"
                shift 2
                ;;
            --max)
                MAX_VALUE="$2"
                shift 2
                ;;
            --values)
                ALLOWED_VALUES="$2"
                shift 2
                ;;
            --predicate)
                PREDICATE="$2"
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
                elif [ -z "$VALIDATION_TYPE" ]; then
                    VALIDATION_TYPE="$1"
                else
                    error "Too many arguments" 1
                fi
                shift
                ;;
        esac
    done

    [ -z "$LANGUAGE" ] || [ -z "$VALIDATION_TYPE" ] && error "Missing required arguments" 1
    echo "$SUPPORTED_LANGUAGES" | grep -wq "$LANGUAGE" || error "Unsupported language: $LANGUAGE" 1
    echo "$SUPPORTED_TYPES" | grep -wq "$VALIDATION_TYPE" || error "Unsupported type: $VALIDATION_TYPE" 1
}

# C++ validators
gen_cpp_range() {
    cat << EOF
if (${VAR_NAME} < ${MIN_VALUE} || ${VAR_NAME} > ${MAX_VALUE}) {
    throw std::out_of_range("${VAR_NAME} must be between ${MIN_VALUE} and ${MAX_VALUE}");
}
EOF
}

gen_cpp_string_empty() {
    cat << EOF
if (${VAR_NAME}.empty()) {
    throw std::invalid_argument("${VAR_NAME} cannot be empty");
}
EOF
}

gen_cpp_email() {
    cat << EOF
std::regex email_pattern(R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$)");
if (!std::regex_match(${VAR_NAME}, email_pattern)) {
    throw std::invalid_argument("${VAR_NAME} is not a valid email");
}
EOF
}

gen_cpp_enum() {
    local values_list=$(echo "$ALLOWED_VALUES" | sed 's/,/", "/g')
    cat << EOF
const std::set<std::string> allowed_values = {"$values_list"};
if (allowed_values.find(${VAR_NAME}) == allowed_values.end()) {
    throw std::invalid_argument("Invalid ${VAR_NAME}");
}
EOF
}

gen_cpp_custom() {
    cat << EOF
if (!(${PREDICATE})) {
    throw std::invalid_argument("Validation failed: ${PREDICATE}");
}
EOF
}

# Go validators
gen_go_range() {
    cat << EOF
if ${VAR_NAME} < ${MIN_VALUE} || ${VAR_NAME} > ${MAX_VALUE} {
    return fmt.Errorf("${VAR_NAME} must be between ${MIN_VALUE} and ${MAX_VALUE}")
}
EOF
}

gen_go_string_empty() {
    cat << EOF
if ${VAR_NAME} == "" {
    return errors.New("${VAR_NAME} cannot be empty")
}
EOF
}

gen_go_email() {
    cat << EOF
emailPattern := regexp.MustCompile(\`^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\\.[a-zA-Z]{2,}$\`)
if !emailPattern.MatchString(${VAR_NAME}) {
    return fmt.Errorf("${VAR_NAME} is not a valid email")
}
EOF
}

gen_go_enum() {
    local values_arr=$(echo "$ALLOWED_VALUES" | sed 's/,/", "/g')
    cat << EOF
allowedValues := map[string]bool{"$values_arr": true}
if !allowedValues[${VAR_NAME}] {
    return fmt.Errorf("invalid ${VAR_NAME}")
}
EOF
}

gen_go_custom() {
    cat << EOF
if !(${PREDICATE}) {
    return fmt.Errorf("validation failed: ${PREDICATE}")
}
EOF
}

# Python validators
gen_python_range() {
    cat << EOF
if not (${MIN_VALUE} <= ${VAR_NAME} <= ${MAX_VALUE}):
    raise ValueError(f"${VAR_NAME} must be between ${MIN_VALUE} and ${MAX_VALUE}")
EOF
}

gen_python_string_empty() {
    cat << EOF
if not ${VAR_NAME}:
    raise ValueError("${VAR_NAME} cannot be empty")
EOF
}

gen_python_email() {
    cat << EOF
import re
email_pattern = r'^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$'
if not re.match(email_pattern, ${VAR_NAME}):
    raise ValueError(f"${VAR_NAME} is not a valid email")
EOF
}

gen_python_enum() {
    local values_set=$(echo "$ALLOWED_VALUES" | sed 's/,/", "/g')
    cat << EOF
allowed_values = {"$values_set"}
if ${VAR_NAME} not in allowed_values:
    raise ValueError(f"Invalid ${VAR_NAME}: {${VAR_NAME}}. Must be one of {allowed_values}")
EOF
}

gen_python_custom() {
    cat << EOF
if not (${PREDICATE}):
    raise ValueError(f"Validation failed: ${PREDICATE}")
EOF
}

# Rust validators
gen_rust_range() {
    cat << EOF
if ${VAR_NAME} < ${MIN_VALUE} || ${VAR_NAME} > ${MAX_VALUE} {
    return Err(format!("${VAR_NAME} must be between ${MIN_VALUE} and ${MAX_VALUE}"));
}
EOF
}

gen_rust_string_empty() {
    cat << EOF
if ${VAR_NAME}.is_empty() {
    return Err("${VAR_NAME} cannot be empty".to_string());
}
EOF
}

gen_rust_email() {
    cat << EOF
let email_pattern = regex::Regex::new(r"^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$").unwrap();
if !email_pattern.is_match(${VAR_NAME}) {
    return Err(format!("${VAR_NAME} is not a valid email"));
}
EOF
}

gen_rust_enum() {
    local values_arr=$(echo "$ALLOWED_VALUES" | sed 's/,/", "/g')
    cat << EOF
let allowed_values = vec!["$values_arr"];
if !allowed_values.contains(&${VAR_NAME}) {
    return Err(format!("invalid ${VAR_NAME}"));
}
EOF
}

gen_rust_custom() {
    cat << EOF
if !(${PREDICATE}) {
    return Err(format!("validation failed: ${PREDICATE}"));
}
EOF
}

# TypeScript validators
gen_typescript_range() {
    cat << EOF
if (${VAR_NAME} < ${MIN_VALUE} || ${VAR_NAME} > ${MAX_VALUE}) {
    throw new RangeError(\`${VAR_NAME} must be between ${MIN_VALUE} and ${MAX_VALUE}\`);
}
EOF
}

gen_typescript_string_empty() {
    cat << EOF
if (!${VAR_NAME} || ${VAR_NAME}.trim() === '') {
    throw new Error("${VAR_NAME} cannot be empty");
}
EOF
}

gen_typescript_email() {
    cat << EOF
const emailPattern = /^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$/;
if (!emailPattern.test(${VAR_NAME})) {
    throw new Error(\`${VAR_NAME} is not a valid email\`);
}
EOF
}

gen_typescript_enum() {
    local values_arr=$(echo "$ALLOWED_VALUES" | sed 's/,/", "/g')
    cat << EOF
const allowedValues = ["$values_arr"];
if (!allowedValues.includes(${VAR_NAME})) {
    throw new Error(\`Invalid ${VAR_NAME}: \${${VAR_NAME}}\`);
}
EOF
}

gen_typescript_custom() {
    cat << EOF
if (!(${PREDICATE})) {
    throw new Error(\`Validation failed: ${PREDICATE}\`);
}
EOF
}

generate_validator() {
    local func_name="gen_${LANGUAGE}_${VALIDATION_TYPE}"
    func_name=$(echo "$func_name" | tr '-' '_')

    if type "$func_name" > /dev/null 2>&1; then
        $func_name
    else
        error "Validator not implemented: $LANGUAGE $VALIDATION_TYPE" 1
    fi
}

main() {
    parse_args "$@"

    local output=$(generate_validator)

    if [ -n "$OUTPUT_FILE" ]; then
        echo "$output" > "$OUTPUT_FILE"
        echo -e "${GREEN}✓${NC} Generated $LANGUAGE $VALIDATION_TYPE validator: $OUTPUT_FILE" >&2
    else
        echo "$output"
    fi

    exit 0
}

main "$@"
