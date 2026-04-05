#!/bin/bash
# test-safe-insert.sh - Test suite for safe-insert.sh
# TDD approach: Write tests first, then implement

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Setup test environment
setup() {
    TEST_DIR=$(mktemp -d)
    TEST_FILE="$TEST_DIR/test.txt"
    CODE_FILE="$TEST_DIR/code.txt"
    SCRIPT="../scripts/safe-insert.sh"

    cat > "$TEST_FILE" << 'EOF'
line 1
line 2
line 3
line 4
line 5
EOF

    echo "inserted code" > "$CODE_FILE"
}

# Teardown test environment
teardown() {
    rm -rf "$TEST_DIR"
}

# Test helper functions
assert_equals() {
    local expected="$1"
    local actual="$2"
    local test_name="$3"

    TESTS_RUN=$((TESTS_RUN + 1))

    if [ "$expected" = "$actual" ]; then
        echo -e "${GREEN}✓${NC} $test_name"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}✗${NC} $test_name"
        echo "  Expected: $expected"
        echo "  Actual: $actual"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

assert_file_exists() {
    local file="$1"
    local test_name="$2"

    TESTS_RUN=$((TESTS_RUN + 1))

    if [ -f "$file" ]; then
        echo -e "${GREEN}✓${NC} $test_name"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}✗${NC} $test_name"
        echo "  File not found: $file"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

assert_line_count() {
    local file="$1"
    local expected="$2"
    local test_name="$3"

    TESTS_RUN=$((TESTS_RUN + 1))

    local actual=$(wc -l < "$file" | tr -d ' ')

    if [ "$expected" = "$actual" ]; then
        echo -e "${GREEN}✓${NC} $test_name"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}✗${NC} $test_name"
        echo "  Expected line count: $expected"
        echo "  Actual line count: $actual"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

assert_line_content() {
    local file="$1"
    local line_num="$2"
    local expected="$3"
    local test_name="$4"

    TESTS_RUN=$((TESTS_RUN + 1))

    local actual=$(sed -n "${line_num}p" "$file")

    if [ "$expected" = "$actual" ]; then
        echo -e "${GREEN}✓${NC} $test_name"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}✗${NC} $test_name"
        echo "  Expected (line $line_num): $expected"
        echo "  Actual (line $line_num): $actual"
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

# Test 1: Insert at beginning (line 1)
test_insert_at_beginning() {
    $SCRIPT "$TEST_FILE" 1 "$CODE_FILE"
    assert_line_content "$TEST_FILE" 1 "inserted code" "Insert at line 1"
    assert_line_content "$TEST_FILE" 2 "line 1" "Original line 1 moved to line 2"
    assert_line_count "$TEST_FILE" 6 "Total lines after insert"
}

# Test 2: Insert in middle (line 3)
test_insert_in_middle() {
    $SCRIPT "$TEST_FILE" 3 "$CODE_FILE"
    assert_line_content "$TEST_FILE" 3 "inserted code" "Insert at line 3"
    assert_line_content "$TEST_FILE" 4 "line 3" "Original line 3 moved to line 4"
    assert_line_count "$TEST_FILE" 6 "Total lines after insert"
}

# Test 3: Insert at end
test_insert_at_end() {
    local line_count=$(wc -l < "$TEST_FILE" | tr -d ' ')
    local insert_line=$((line_count + 1))
    $SCRIPT "$TEST_FILE" "$insert_line" "$CODE_FILE"
    assert_line_content "$TEST_FILE" "$insert_line" "inserted code" "Insert at end"
    assert_line_count "$TEST_FILE" 6 "Total lines after insert at end"
}

# Test 4: Backup file created
test_backup_created() {
    rm -f "$TEST_FILE.bak"
    $SCRIPT "$TEST_FILE" 1 "$CODE_FILE"
    assert_file_exists "$TEST_FILE.bak" "Backup file created"
}

# Test 5: Dry-run does not modify file
test_dry_run() {
    cp "$TEST_FILE" "$TEST_FILE.original"
    $SCRIPT --dry-run "$TEST_FILE" 3 "$CODE_FILE"

    # Compare files
    if diff -q "$TEST_FILE" "$TEST_FILE.original" > /dev/null; then
        TESTS_RUN=$((TESTS_RUN + 1))
        TESTS_PASSED=$((TESTS_PASSED + 1))
        echo -e "${GREEN}✓${NC} Dry-run does not modify file"
    else
        TESTS_RUN=$((TESTS_RUN + 1))
        TESTS_FAILED=$((TESTS_FAILED + 1))
        echo -e "${RED}✗${NC} Dry-run does not modify file"
    fi

    rm "$TEST_FILE.original"
}

# Test 6: File not found returns exit code 1
test_file_not_found() {
    EXIT_CODE=0
    $SCRIPT "$TEST_DIR/nonexistent.txt" 1 "$CODE_FILE" 2>/dev/null || EXIT_CODE=$?
    assert_exit_code 1 "$EXIT_CODE" "File not found returns exit code 1"
}

# Test 7: Code file not found returns exit code 1
test_code_file_not_found() {
    EXIT_CODE=0
    $SCRIPT "$TEST_FILE" 1 "$TEST_DIR/nonexistent.txt" 2>/dev/null || EXIT_CODE=$?
    assert_exit_code 1 "$EXIT_CODE" "Code file not found returns exit code 1"
}

# Test 8: Line number out of range returns exit code 2
test_line_out_of_range() {
    EXIT_CODE=0
    $SCRIPT "$TEST_FILE" 999 "$CODE_FILE" 2>/dev/null || EXIT_CODE=$?
    assert_exit_code 2 "$EXIT_CODE" "Line out of range returns exit code 2"
}

# Test 9: Insert from stdin
test_insert_from_stdin() {
    echo "stdin code" | $SCRIPT "$TEST_FILE" 2 -
    assert_line_content "$TEST_FILE" 2 "stdin code" "Insert from stdin"
}

# Test 10: Insert with auto-indent
test_insert_with_indent() {
    echo "code" > "$CODE_FILE"
    $SCRIPT "$TEST_FILE" 2 "$CODE_FILE" --indent 4
    assert_line_content "$TEST_FILE" 2 "    code" "Insert with 4-space indent"
}

# Test 11: Insert multiline code
test_insert_multiline() {
    cat > "$CODE_FILE" << 'EOF'
first line
second line
third line
EOF
    $SCRIPT "$TEST_FILE" 2 "$CODE_FILE"
    assert_line_content "$TEST_FILE" 2 "first line" "Multiline insert: line 1"
    assert_line_content "$TEST_FILE" 3 "second line" "Multiline insert: line 2"
    assert_line_content "$TEST_FILE" 4 "third line" "Multiline insert: line 3"
    assert_line_count "$TEST_FILE" 8 "Total lines after multiline insert"
}

# Test 12: Custom backup suffix
test_custom_backup_suffix() {
    rm -f "$TEST_FILE.backup"
    $SCRIPT "$TEST_FILE" 1 "$CODE_FILE" --backup ".backup"
    assert_file_exists "$TEST_FILE.backup" "Custom backup suffix works"
}

# Test 13: Validation command runs
test_validation_command() {
    EXIT_CODE=0
    $SCRIPT "$TEST_FILE" 1 "$CODE_FILE" --validate "echo 'validation passed'" || EXIT_CODE=$?
    assert_exit_code 0 "$EXIT_CODE" "Validation command runs successfully"
}

# Test 14: Failed validation returns exit code 3
test_validation_failed() {
    EXIT_CODE=0
    $SCRIPT "$TEST_FILE" 1 "$CODE_FILE" --validate "false" 2>/dev/null || EXIT_CODE=$?
    assert_exit_code 3 "$EXIT_CODE" "Failed validation returns exit code 3"
}

# Test 15: Help flag
test_help_flag() {
    EXIT_CODE=0
    $SCRIPT --help > /dev/null 2>&1 || EXIT_CODE=$?
    assert_exit_code 0 "$EXIT_CODE" "Help flag works"
}

# Test 16: Insert at line 0 fails (invalid)
test_insert_at_line_zero() {
    EXIT_CODE=0
    $SCRIPT "$TEST_FILE" 0 "$CODE_FILE" 2>/dev/null || EXIT_CODE=$?
    assert_exit_code 2 "$EXIT_CODE" "Line 0 is invalid"
}

# Test 17: Preserves file permissions
test_preserves_permissions() {
    chmod 600 "$TEST_FILE"
    ORIGINAL_PERMS=$(stat -f "%A" "$TEST_FILE" 2>/dev/null || stat -c "%a" "$TEST_FILE" 2>/dev/null)
    $SCRIPT "$TEST_FILE" 1 "$CODE_FILE"
    NEW_PERMS=$(stat -f "%A" "$TEST_FILE" 2>/dev/null || stat -c "%a" "$TEST_FILE" 2>/dev/null)
    assert_equals "$ORIGINAL_PERMS" "$NEW_PERMS" "File permissions preserved"
}

# Main test runner
main() {
    echo "Running safe-insert.sh test suite..."
    echo ""

    setup

    # Run all tests
    test_insert_at_beginning
    setup  # Reset for next test
    test_insert_in_middle
    setup
    test_insert_at_end
    setup
    test_backup_created
    setup
    test_dry_run
    setup
    test_file_not_found
    setup
    test_code_file_not_found
    setup
    test_line_out_of_range
    setup
    test_insert_from_stdin
    setup
    test_insert_with_indent
    setup
    test_insert_multiline
    setup
    test_custom_backup_suffix
    setup
    test_validation_command
    setup
    test_validation_failed
    setup
    test_help_flag
    setup
    test_insert_at_line_zero
    setup
    test_preserves_permissions

    teardown

    # Print summary
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

# Change to tests directory
cd "$(dirname "$0")"

# Run tests
main
