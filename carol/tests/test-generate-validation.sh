#!/bin/bash
# test-generate-validation.sh - Test suite for generate-validation.sh

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

SCRIPT="../scripts/generate-validation.sh"

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
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

# Test 1: C++ range validation
test_cpp_range() {
    OUTPUT=$($SCRIPT cpp range --var volume --min 0.0 --max 1.0)
    assert_contains "$OUTPUT" "volume < 0.0" "C++ range checks minimum"
    assert_contains "$OUTPUT" "volume > 1.0" "C++ range checks maximum"
}

# Test 2: Go string-empty
test_go_string_empty() {
    OUTPUT=$($SCRIPT go string-empty --var username)
    assert_contains "$OUTPUT" 'username == ""' "Go checks empty string"
}

# Test 3: Python enum
test_python_enum() {
    OUTPUT=$($SCRIPT python enum --var status --values "pending,active,done")
    assert_contains "$OUTPUT" "pending" "Python enum includes pending"
    assert_contains "$OUTPUT" "active" "Python enum includes active"
    assert_contains "$OUTPUT" "done" "Python enum includes done"
}

# Test 4: Rust email
test_rust_email() {
    OUTPUT=$($SCRIPT rust email --var email)
    assert_contains "$OUTPUT" "@" "Rust email validation checks @"
}

# Test 5: TypeScript custom predicate
test_typescript_custom() {
    OUTPUT=$($SCRIPT typescript custom --var age --predicate "age >= 18")
    assert_contains "$OUTPUT" "age >= 18" "TypeScript custom predicate used"
}

# Test 6: Invalid language
test_invalid_language() {
    EXIT_CODE=0
    $SCRIPT invalid range 2>/dev/null || EXIT_CODE=$?
    assert_exit_code 1 "$EXIT_CODE" "Invalid language"
}

# Test 7: Invalid type
test_invalid_type() {
    EXIT_CODE=0
    $SCRIPT cpp invalid_type 2>/dev/null || EXIT_CODE=$?
    assert_exit_code 1 "$EXIT_CODE" "Invalid validation type"
}

# Test 8: Help flag
test_help() {
    EXIT_CODE=0
    $SCRIPT --help > /dev/null 2>&1 || EXIT_CODE=$?
    assert_exit_code 0 "$EXIT_CODE" "Help flag"
}

main() {
    echo "Running generate-validation.sh test suite..."
    echo ""

    cd "$(dirname "$0")"

    test_cpp_range
    test_go_string_empty
    test_python_enum
    test_rust_email
    test_typescript_custom
    test_invalid_language
    test_invalid_type
    test_help

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
