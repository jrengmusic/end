#pragma once
#include <juce_core/juce_core.h>
#include "jreng_markdown_types.h"
#include "jreng_markdown_parser.h"

namespace jreng::Markdown
{
/*____________________________________________________________________________*/

enum class Alignment
{
    None,
    Left,
    Center,
    Right
};

using StyledText = std::vector<InlineSpan>;

struct TableCell
{
    int row { 0 };
    int col { 0 };
    bool isHeader { false };
    Alignment align { Alignment::None };

    juce::String text;
    StyledText tokens;
};

struct Table
{
    int startLine { 0 };
    int lineCount { 0 };
    int columnCount { 0 };

    int headerRowCount { 1 };
    int bodyRowCount { 0 };

    std::vector<TableCell> cells;
};

using Tables = std::vector<Table>;

Tables parseTables (const juce::String& markdown);

bool lineHasUnescapedPipe (const juce::String& line);

juce::StringArray splitTableRow (const juce::String& line);

std::vector<Alignment> parseAlignmentRow (const juce::String& line);

Tables parseTablesImpl (const juce::StringArray& lines);

/**_____________________________END OF NAMESPACE______________________________*/
} /** namespace jreng::Markdown */
