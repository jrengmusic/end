/** ========================================================================================

                            Binary Data generated with Binaria

    ======================================================================================== */

#pragma once

#include <utility>

namespace jreng::Colours
{
/*____________________________________________________________________________*/

extern const char* names_json;
const int names_jsonSize = 115762;

extern const char* getNamedResource (const char* resourceNameUTF8, int& numBytes);
extern const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);

extern const char* namedResourceList[];
extern const char* originalFilenames[];

extern const char* getResourceByFilenameRaw (const char* filenameUTF8, int& numBytes);
extern std::pair<const void*, int> getResourceByFilename (const char* filenameUTF8);

/**_____________________________END OF NAMESPACE______________________________*/
} /** namespace jreng::Colours */
