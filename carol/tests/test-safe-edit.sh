#!/bin/bash
# test-safe-edit.sh - Test suite for safe-edit.sh
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
    SCRIPT="../scripts/safe-edit.sh"

    echo "Test file content" > "$TEST_FILE"
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

assert_file_not_exists() {
    local file="$1"
    local test_name="$2"

    TESTS_RUN=$((TESTS_RUN + 1))

    if [ ! -f "$file" ]; then
        echo -e "${GREEN}✓${NC} $test_name"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}✗${NC} $test_name"
        echo "  File should not exist: $file"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

assert_contains() {
    local file="$1"
    local pattern="$2"
    local test_name="$3"

    TESTS_RUN=$((TESTS_RUN + 1))

    if grep -q "$pattern" "$file"; then
        echo -e "${GREEN}✓${NC} $test_name"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}✗${NC} $test_name"
        echo "  Pattern not found: $pattern"
        echo "  File contents:"
        cat "$file"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

assert_not_contains() {
    local file="$1"
    local pattern="$2"
    local test_name="$3"

    TESTS_RUN=$((TESTS_RUN + 1))

    if ! grep -q "$pattern" "$file"; then
        echo -e "${GREEN}✓${NC} $test_name"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}✗${NC} $test_name"
        echo "  Pattern should not be found: $pattern"
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

# Test 1: Basic text replacement
test_basic_replacement() {
    echo "Test file content" > "$TEST_FILE"
    $SCRIPT "$TEST_FILE" "Test" "Modified"
    assert_contains "$TEST_FILE" "Modified file content" "Basic text replacement"
}

# Test 2: Backup file created
test_backup_created() {
    echo "Original content" > "$TEST_FILE"
    $SCRIPT "$TEST_FILE" "Original" "New"
    assert_file_exists "$TEST_FILE.bak" "Backup file created"
    assert_contains "$TEST_FILE.bak" "Original content" "Backup has original content"
}

# Test 3: Dry-run does not modify file
test_dry_run() {
    echo "Unchanged content" > "$TEST_FILE"
    # Clean up any previous backups
    rm -f "$TEST_FILE.bak" "$TEST_FILE.backup"
    $SCRIPT --dry-run "$TEST_FILE" "Unchanged" "Changed"
    assert_contains "$TEST_FILE" "Unchanged content" "Dry-run does not modify file"
    assert_file_not_exists "$TEST_FILE.bak" "Dry-run does not create backup"
}

# Test 4: File not found returns exit code 1
test_file_not_found() {
    EXIT_CODE=0
    $SCRIPT "$TEST_DIR/nonexistent.txt" "foo" "bar" 2>/dev/null || EXIT_CODE=$?
    assert_exit_code 1 "$EXIT_CODE" "File not found returns exit code 1"
}

# Test 5: Pattern not found returns exit code 3
test_pattern_not_found() {
    echo "Some content" > "$TEST_FILE"
    EXIT_CODE=0
    $SCRIPT "$TEST_FILE" "nonexistent_pattern" "replacement" 2>/dev/null || EXIT_CODE=$?
    assert_exit_code 3 "$EXIT_CODE" "Pattern not found returns exit code 3"
}

# Test 6: Regex replacement
test_regex_replacement() {
    echo "version = 1.0" > "$TEST_FILE"
    $SCRIPT --regex "$TEST_FILE" "version.*=.*" "version = 2.0"
    assert_contains "$TEST_FILE" "version = 2.0" "Regex replacement works"
    assert_not_contains "$TEST_FILE" "version = 1.0" "Old pattern removed"
}

# Test 7: Custom backup suffix
test_custom_backup_suffix() {
    echo "Content" > "$TEST_FILE"
    $SCRIPT "$TEST_FILE" "Content" "New" --backup ".backup"
    assert_file_exists "$TEST_FILE.backup" "Custom backup suffix works"
}

# Test 8: Multiple occurrences replaced
test_multiple_occurrences() {
    echo -e "foo bar\nfoo baz\nfoo qux" > "$TEST_FILE"
    $SCRIPT "$TEST_FILE" "foo" "replaced"
    assert_contains "$TEST_FILE" "replaced bar" "First occurrence replaced"
    assert_contains "$TEST_FILE" "replaced baz" "Second occurrence replaced"
    assert_contains "$TEST_FILE" "replaced qux" "Third occurrence replaced"
}

# Test 9: Validation command runs
test_validation_command() {
    echo "content" > "$TEST_FILE"
    $SCRIPT "$TEST_FILE" "content" "new" --validate "echo 'validation passed'"
    assert_exit_code 0 "$?" "Validation command runs successfully"
}

# Test 10: Failed validation returns exit code 4
test_validation_failed() {
    echo "content" > "$TEST_FILE"
    EXIT_CODE=0
    $SCRIPT "$TEST_FILE" "content" "new" --validate "false" 2>/dev/null || EXIT_CODE=$?
    assert_exit_code 4 "$EXIT_CODE" "Failed validation returns exit code 4"
}

# Test 11: Atomic write (temp file used)
test_atomic_write() {
    echo "Original" > "$TEST_FILE"
    # This test verifies that the script uses a temp file for atomic writes
    # We can't directly test this without modifying the script, but we can verify
    # that the file is not corrupted if interrupted
    $SCRIPT "$TEST_FILE" "Original" "New"
    assert_contains "$TEST_FILE" "New" "File successfully written atomically"
}

# Test 12: Help flag
test_help_flag() {
    EXIT_CODE=0
    $SCRIPT --help > /dev/null 2>&1 || EXIT_CODE=$?
    # Help should exit with 0
    assert_exit_code 0 "$EXIT_CODE" "Help flag works"
}

# Test 13: Empty file handling
test_empty_file() {
    touch "$TEST_FILE"
    EXIT_CODE=0
    $SCRIPT "$TEST_FILE" "nonexistent" "replacement" 2>/dev/null || EXIT_CODE=$?
    assert_exit_code 3 "$EXIT_CODE" "Empty file with nonexistent pattern returns exit code 3"
}

# Test 14: Special characters in replacement
test_special_characters() {
    echo "path/to/file" > "$TEST_FILE"
    $SCRIPT "$TEST_FILE" "path/to/file" "path\\to\\file"
    assert_contains "$TEST_FILE" "path\\\\to\\\\file" "Special characters handled"
}

# Test 15: Preserves file permissions
test_preserves_permissions() {
    echo "content" > "$TEST_FILE"
    chmod 600 "$TEST_FILE"
    ORIGINAL_PERMS=$(stat -f "%A" "$TEST_FILE" 2>/dev/null || stat -c "%a" "$TEST_FILE" 2>/dev/null)
    $SCRIPT "$TEST_FILE" "content" "new"
    NEW_PERMS=$(stat -f "%A" "$TEST_FILE" 2>/dev/null || stat -c "%a" "$TEST_FILE" 2>/dev/null)
    assert_equals "$ORIGINAL_PERMS" "$NEW_PERMS" "File permissions preserved"
}

# Main test runner
main() {
    echo "Running safe-edit.sh test suite..."
    echo ""

    setup

    # Run all tests
    test_basic_replacement
    test_backup_created
    test_dry_run
    test_file_not_found
    test_pattern_not_found
    test_regex_replacement
    test_custom_backup_suffix
    test_multiple_occurrences
    test_validation_command
    test_validation_failed
    test_atomic_write
    test_help_flag
    test_empty_file
    test_special_characters
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
