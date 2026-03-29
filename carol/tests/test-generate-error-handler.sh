#!/bin/bash
# test-generate-error-handler.sh - Test suite for generate-error-handler.sh
# TDD approach: Focused tests for pattern generator

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

SCRIPT="../scripts/generate-error-handler.sh"

assert_contains() {
    local output="$1"
    local pattern="$2"
    local test_name="$3"

    TESTS_RUN=$((TESTS_RUN + 1))

    if echo "$output" | grep -q "$pattern"; then
        echo -e "${GREEN}✓${NC} $test_name"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}✗${NC} $test_name"
        echo "  Pattern not found: $pattern"
        echo "  Output: $output"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

assert_exit_code() {
    local expected="$1"
    local actual="$2"
    local test_name="$3"

    TESTS_RUN=$((TESTS_RUN + 1))

    if [ "$expected" -eq "$actual" ]; then
        echo -e "${GREEN}✓${NC} $test_name"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}✗${NC} $test_name"
        echo "  Expected exit code: $expected"
        echo "  Actual exit code: $actual"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

# Test 1: C++ null-check
test_cpp_null_check() {
    OUTPUT=$($SCRIPT cpp null-check --var user)
    assert_contains "$OUTPUT" "if (!user)" "C++ null-check generated"
    assert_contains "$OUTPUT" "throw" "C++ null-check throws exception"
}

# Test 2: Go bounds-check
test_go_bounds_check() {
    OUTPUT=$($SCRIPT go bounds-check --var items)
    assert_contains "$OUTPUT" "len(items)" "Go bounds-check uses len()"
    assert_contains "$OUTPUT" "index" "Go bounds-check checks index"
}

# Test 3: Python file-error
test_python_file_error() {
    OUTPUT=$($SCRIPT python file-error --var path)
    assert_contains "$OUTPUT" "try:" "Python file-error has try block"
    assert_contains "$OUTPUT" "FileNotFoundError" "Python file-error handles FileNotFoundError"
}

# Test 4: Rust network-error
test_rust_network_error() {
    OUTPUT=$($SCRIPT rust network-error)
    assert_contains "$OUTPUT" "match" "Rust network-error uses match"
    assert_contains "$OUTPUT" "Err" "Rust network-error handles Err"
}

# Test 5: TypeScript parse-error
test_typescript_parse_error() {
    OUTPUT=$($SCRIPT typescript parse-error --var data)
    assert_contains "$OUTPUT" "try" "TypeScript parse-error has try block"
    assert_contains "$OUTPUT" "catch" "TypeScript parse-error has catch block"
}

# Test 6: Invalid language
test_invalid_language() {
    EXIT_CODE=0
    $SCRIPT invalid_lang null-check 2>/dev/null || EXIT_CODE=$?
    assert_exit_code 1 "$EXIT_CODE" "Invalid language returns exit code 1"
}

# Test 7: Invalid type
test_invalid_type() {
    EXIT_CODE=0
    $SCRIPT cpp invalid_type 2>/dev/null || EXIT_CODE=$?
    assert_exit_code 1 "$EXIT_CODE" "Invalid error type returns exit code 1"
}

# Test 8: Help flag
test_help_flag() {
    EXIT_CODE=0
    $SCRIPT --help > /dev/null 2>&1 || EXIT_CODE=$?
    assert_exit_code 0 "$EXIT_CODE" "Help flag works"
}

# Test 9: Output to file
test_output_to_file() {
    TEMP_FILE=$(mktemp)
    $SCRIPT cpp null-check --var ptr --output "$TEMP_FILE"

    if [ -f "$TEMP_FILE" ] && grep -q "if (!ptr)" "$TEMP_FILE"; then
        TESTS_RUN=$((TESTS_RUN + 1))
        TESTS_PASSED=$((TESTS_PASSED + 1))
        echo -e "${GREEN}✓${NC} Output to file works"
    else
        TESTS_RUN=$((TESTS_RUN + 1))
        TESTS_FAILED=$((TESTS_FAILED + 1))
        echo -e "${RED}✗${NC} Output to file works"
    fi

    rm -f "$TEMP_FILE"
}

# Test 10: Custom function name
test_custom_function_name() {
    OUTPUT=$($SCRIPT cpp null-check --var user --function validateUser)
    assert_contains "$OUTPUT" "validateUser" "Custom function name included in comment"
}

# Main
main() {
    echo "Running generate-error-handler.sh test suite..."
    echo ""

    cd "$(dirname "$0")"

    test_cpp_null_check
    test_go_bounds_check
    test_python_file_error
    test_rust_network_error
    test_typescript_parse_error
    test_invalid_language
    test_invalid_type
    test_help_flag
    test_output_to_file
    test_custom_function_name

    echo ""
    echo "=========================================="
    echo "Test Summary:"
    echo "  Total: $TESTS_RUN"
    echo -e "  ${GREEN}Passed: $TESTS_PASSED${NC}"
    if [ $TESTS_FAILED -gt 0 ]; then
        echo -e "  ${RED}Failed: $TESTS_FAILED${NC}"
        exit 1
    else
        echo -e "  ${GREEN}All tests passed!${NC}"
        exit 0
    fi
}

main
