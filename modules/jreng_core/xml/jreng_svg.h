namespace jreng
{
/*____________________________________________________________________________*/

struct SVG
{
    /** -------------------- FUNCTIONS FOR READING SVG ---------------------------*/
    struct Filter
    {
        /**
         * @brief Remove outer <g> group tags from an SVG element.
         *
         * This function takes a juce::XmlElement representing an SVG fragment,
         * converts it to a string, and strips away all <g> opening tags and the
         * final closing </g> tag. The result is the raw inner content of the
         * group, without any group wrappers.
         *
         * @param svg Pointer to a juce::XmlElement containing the SVG fragment.
         *            Must not be nullptr.
         *
         * @return A juce::String containing the SVG markup with all <g> tags removed.
         *
         * @note This is useful when you want to flatten grouped SVG content into
         *       a single layer, or when embedding fragments into a larger SVG
         *       document without redundant grouping.
         */
        static const juce::String group (juce::XmlElement* svg);
        
        /**
         * @brief Remove XML declaration and redundant attributes from an SVG element.
         *
         * This function takes a juce::XmlElement representing an SVG fragment,
         * converts it to a string, and strips away:
         * - The XML declaration (`<?xml ... ?>`) and any content before the <svg> tag.
         * - The "version" attribute inside the <svg> tag.
         *
         * The result is a cleaner SVG string suitable for embedding into larger
         * documents or for serialization without redundant headers.
         *
         * @param svg Pointer to a juce::XmlElement containing the SVG fragment.
         *            Must not be nullptr.
         *
         * @return A juce::String containing the SVG markup with the XML declaration
         *         and version attribute removed, trimmed of leading/trailing whitespace.
         *
         * @note This is useful when exporting SVG fragments that will be combined
         *       into a single document, where multiple XML declarations or version
         *       attributes would be invalid.
         */
        static const juce::String declaration (juce::XmlElement* svg);
    };

    //==============================================================================
    /** -------------------- FUNCTIONS FOR DRAWING SVG ---------------------------*/
    /**
     * @brief Draw an SVG file into a graphics context.
     *
     * Loads an SVG from a file, parses it into a juce::Drawable, and renders it
     * within the specified area of the given Graphics context.
     *
     * @param g     Reference to the juce::Graphics context to draw into.
     * @param area  Target rectangle in which the SVG will be drawn.
     * @param file  The juce::File containing the SVG data.
     *
     * @note The SVG is scaled and centred within the target area.
     */
    static void draw (juce::Graphics& g, juce::Rectangle<int> area, const juce::File& file);
    
    /**
     * @brief Draw an SVG element into a graphics context.
     *
     * Renders a juce::XmlElement representing an SVG fragment into the specified
     * area of the given Graphics context.
     *
     * @param g     Reference to the juce::Graphics context to draw into.
     * @param area  Target rectangle in which the SVG will be drawn.
     * @param e     Pointer to a juce::XmlElement containing the SVG data.
     *
     * @note The SVG is scaled and centred within the target area.
     *       The XmlElement must not be nullptr.
     */
    static void draw (juce::Graphics& g, juce::Rectangle<int> area, juce::XmlElement* e);
    
    /**
     * @brief Draw a juce::Drawable into a graphics context.
     *
     * Renders a juce::Drawable object directly into the specified area of the
     * given Graphics context.
     *
     * @param g         Reference to the juce::Graphics context to draw into.
     * @param area      Target rectangle in which the Drawable will be drawn.
     * @param drawable  Pointer to a juce::Drawable object to render.
     *
     * @note The Drawable is scaled and centred within the target area.
     *       The drawable pointer must not be nullptr.
     */
    static void draw (juce::Graphics& g, juce::Rectangle<int> area, juce::Drawable* drawable);
    
    /**
     * @brief Extract ellipse paths from an SVG element.
     *
     * Iterates through all child elements of the given XmlElement with the tag
     * name "ellipse", reads their attributes (cx, cy, rx, ry), and constructs
     * corresponding ellipse shapes in a juce::Path.
     *
     * @param xml Pointer to a juce::XmlElement containing one or more <ellipse>
     *            elements. Must not be nullptr.
     *
     * @return A juce::Path containing all ellipses found in the XML element.
     *
     * @note Each ellipse is defined by its centre (cx, cy) and radii (rx, ry).
     *       The resulting path can be used for rendering or hit‑testing.
     */
    static const juce::Path getEllipsePath (juce::XmlElement* xml);
    
    /**
     * @brief Extract circle paths from an SVG element.
     *
     * Iterates through all child elements of the given XmlElement with the tag
     * name "circle", reads their attributes (cx, cy, r), and constructs
     * corresponding circle shapes in a juce::Path.
     *
     * Each circle is defined by its centre (cx, cy) and radius (r). The function
     * converts these into an ellipse with equal radii, effectively producing a
     * circle path.
     *
     * @param xml Pointer to a juce::XmlElement containing one or more <circle>
     *            elements. Must not be nullptr.
     *
     * @return A juce::Path containing all circles found in the XML element.
     *
     * @note The resulting path can be used for rendering, hit‑testing, or further
     *       geometric processing.
     */
    static const juce::Path getCirclePath (juce::XmlElement* xml);
    
    /**
     * @brief Extract rectangle paths from an SVG element.
     *
     * Iterates through all child elements of the given XmlElement with the tag
     * name "rect", reads their attributes (x, y, width, height), and constructs
     * corresponding rectangle shapes in a juce::Path.
     *
     * Each rectangle is defined by its top‑left corner (x, y) and its dimensions
     * (width, height).
     *
     * @param xml Pointer to a juce::XmlElement containing one or more <rect>
     *            elements. Must not be nullptr.
     *
     * @return A juce::Path containing all rectangles found in the XML element.
     *
     * @note The resulting path can be used for rendering, hit‑testing, or further
     *       geometric processing.
     */
    static const juce::Path getRectPath (juce::XmlElement* xml);

    /**
     * @brief Enumeration of supported SVG element types.
     *
     * This enum class defines the categories of SVG elements that can be
     * parsed, filtered, or converted into juce::Path objects. It is typically
     * used to specify which type(s) of elements should be processed when
     * traversing an SVG document.
     */
    enum class ElementType
    {
        /**
         * @brief Select all supported element types.
         *
         * Use this when you want to include every element type
         * (path, ellipse, circle, rect).
         */
        all,

        /**
         * @brief Represents an SVG <path> element.
         *
         * Arbitrary vector paths defined by SVG path data.
         */
        path,

        /**
         * @brief Represents an SVG <ellipse> element.
         *
         * Defined by centre coordinates (cx, cy) and radii (rx, ry).
         */
        ellipse,

        /**
         * @brief Represents an SVG <circle> element.
         *
         * Defined by centre coordinates (cx, cy) and radius (r).
         */
        circle,

        /**
         * @brief Represents an SVG <rect> element.
         *
         * Defined by top‑left coordinates (x, y) and dimensions (width, height).
         */
        rect,
    };

    /**
     * @brief Collect all SVG shapes of a given type into a single juce::Path.
     *
     * This function inspects the children of the provided XmlElement and extracts
     * geometry based on the specified ElementType. The resulting shapes are
     * appended into a single juce::Path, which can then be rendered, transformed,
     * or used for hit‑testing.
     *
     * Supported element types:
     * - ElementType::path    → Parses <path> elements using their "d" attribute.
     * - ElementType::ellipse → Adds ellipses defined by <ellipse> elements.
     * - ElementType::circle  → Adds circles defined by <circle> elements.
     * - ElementType::rect    → Adds rectangles defined by <rect> elements.
     * - ElementType::all     → Collects all of the above element types.
     *
     * @param xml          Pointer to a juce::XmlElement containing SVG child
     *                     elements. Must not be nullptr.
     * @param elementToAdd The type of SVG element(s) to extract, as defined by
     *                     the ElementType enum.
     *
     * @return A juce::Path containing the combined geometry of all matching
     *         elements found in the XML.
     *
     * @note For ElementType::all, the function sequentially adds paths, ellipses,
     *       circles, and rectangles into the returned juce::Path.
     * @note This function does not perform any styling (stroke, fill, colour);
     *       it only extracts the raw geometry.
     */
    static const juce::Path getAllFoundPath (juce::XmlElement* xml,
                                             ElementType elementToAdd = ElementType::all);

    /**
     * @brief Extract geometry from an SVG element and its groups.
     *
     * Traverses the given XmlElement, collecting geometry of the specified
     * ElementType from both the element itself and any child <g> groups.
     * The resulting shapes are combined into a single juce::Path.
     *
     * @param svg          Pointer to a juce::XmlElement representing the root
     *                     or a fragment of an SVG document. May be nullptr.
     * @param elementToAdd The type of SVG element(s) to extract, as defined by
     *                     the ElementType enum.
     *
     * @return A juce::Path containing the combined geometry of all matching
     *         elements found in the XML. Returns an empty path if svg is nullptr.
     *
     * @note This overload does not apply any scaling or fitting; it extracts
     *       raw geometry only.
     */

    static const juce::Path getPath (juce::XmlElement* svg,
                                     ElementType elementToAdd);

    /**
     * @brief Extract geometry from an SVG string.
     *
     * Parses the given string into an XmlElement, then delegates to the
     * XmlElement overload of getPath() to extract geometry of the specified type.
     *
     * @param svgString    A juce::String containing valid SVG markup.
     * @param elementToAdd The type of SVG element(s) to extract, as defined by
     *                     the ElementType enum.
     *
     * @return A juce::Path containing the combined geometry of all matching
     *         elements found in the parsed SVG string.
     *
     * @note This overload is convenient when working with in‑memory SVG data
     *       rather than files or XmlElement objects.
     */
    static const juce::Path getPath (const juce::String& svgString,
                                     ElementType elementToAdd = ElementType::all);

    /**
     * @brief Extract and fit geometry from an SVG element into a target area.
     *
     * Collects geometry of the specified ElementType from the given XmlElement,
     * then applies a transform to scale and position the geometry so that it
     * fits within the specified target rectangle.
     *
     * The source bounds are determined by:
     * - The "viewBox" attribute, if present.
     * - Otherwise, the "width" and "height" attributes of the SVG element.
     *
     * @param svg          Pointer to a juce::XmlElement representing the root
     *                     or a fragment of an SVG document. Must not be nullptr.
     * @param areaToFit    The rectangle into which the extracted geometry
     *                     should be scaled and fitted.
     * @param elementToAdd The type of SVG element(s) to extract, as defined by
     *                     the ElementType enum.
     *
     * @return A juce::Path containing the transformed geometry.
     *
     * @note This overload is useful when rendering SVG content into a specific
     *       layout area, ensuring correct scaling and aspect ratio.
     */
    static const juce::Path getPath (juce::XmlElement* svg,
                                     const juce::Rectangle<float>& areaToFit,
                                     ElementType elementToAdd = ElementType::all);

    
    /**
     * @brief Extract and fit geometry from an SVG string into a target area.
     *
     * Parses the given SVG string into an XmlElement, extracts geometry of the
     * specified ElementType, and applies a transform so that the geometry fits
     * within the specified target rectangle.
     *
     * The source bounds are determined by:
     * - The "viewBox" attribute, if present.
     * - Otherwise, the "width" and "height" attributes of the SVG element.
     *
     * @param svg          A juce::String containing valid SVG markup.
     * @param areaToFit    The rectangle into which the extracted geometry
     *                     should be scaled and fitted.
     * @param elementToAdd The type of SVG element(s) to extract, as defined by
     *                     the ElementType enum.
     *
     * @return A juce::Path containing the transformed geometry.
     *
     * @note This overload accepts a floating‑point rectangle for precise fitting.
     */
    static const juce::Path getPath (const juce::String& svg,
                                     const juce::Rectangle<float>& areaToFit,
                                     ElementType elementToAdd = ElementType::all);

    /**
     * @brief Extract and fit geometry from an SVG string into a target area.
     *
     * Parses the given SVG string into an XmlElement, extracts geometry of the
     * specified ElementType, and applies a transform so that the geometry fits
     * within the specified target rectangle.
     *
     * The source bounds are determined by:
     * - The "viewBox" attribute, if present.
     * - Otherwise, the "width" and "height" attributes of the SVG element.
     *
     * @param svg          A juce::String containing valid SVG markup.
     * @param areaToFit    The rectangle into which the extracted geometry
     *                     should be scaled and fitted (integer coordinates).
     * @param elementToAdd The type of SVG element(s) to extract, as defined by
     *                     the ElementType enum.
     *
     * @return A juce::Path containing the transformed geometry.
     *
     * @note This overload accepts an integer rectangle, which is internally
     *       converted to a floating‑point rectangle for fitting.
     */
    
    static const juce::Path getPath (const juce::String& svg,
                                     const juce::Rectangle<int>& areaToFit,
                                     ElementType elementToAdd = ElementType::all);

    
    /**
     * @brief Enumeration of SVG path rendering styles.
     *
     * Defines how a path should be represented when converted to SVG or drawn.
     * This is typically used to decide whether a path is rendered with a stroke,
     * a fill, or an alternate style.
     */
    enum class PathStyle
    {
        /**
         * @brief Render the path using stroke attributes.
         *
         * The path outline is drawn with a specified colour and stroke width.
         */
        stroke,

        /**
         * @brief Render the path using fill attributes.
         *
         * The interior of the path is filled with a specified colour.
         */
        fill,

        /**
         * @brief Render the path using an alternate style.
         *
         * This can be used for special cases or custom rendering modes
         * that differ from standard stroke or fill.
         */
        alternate,
    };

#if JUCE_MODULE_AVAILABLE_juce_gui_basics
    /**
     * @brief Create a DrawablePath from an SVG element with a specified style.
     *
     * Extracts geometry from the given SVG XmlElement, wraps it in a juce::DrawablePath,
     * and applies either stroke or fill styling depending on the PathStyle.
     *
     * - PathStyle::stroke    → The path is stroked with the given colour and stroke width.
     * - PathStyle::fill      → The path is filled with the given colour.
     * - PathStyle::alternate → The path uses even‑odd winding (non‑zero winding disabled),
     *                          then falls through to stroke styling.
     *
     * @param svg          Pointer to a juce::XmlElement containing SVG data. Must not be nullptr.
     * @param colour       Colour to use for stroke or fill.
     * @param elementToAdd The type of SVG element(s) to extract, as defined by ElementType.
     * @param style        The rendering style (stroke, fill, or alternate).
     * @param strokeWidth  Stroke width in pixels (only used for stroke/alternate).
     *
     * @return A std::unique_ptr to a juce::DrawablePath configured with the extracted geometry.
     *
     * @note In the PathStyle::alternate case, the path winding rule is set to even‑odd
     *       (`setUsingNonZeroWinding(false)`), then stroke styling is applied.
     */
    static std::unique_ptr<juce::DrawablePath>
    getDrawablePath (juce::XmlElement* svg,
                     const juce::Colour& colour = juce::Colours::white,
                     ElementType elementToAdd = ElementType::all,
                     PathStyle style = PathStyle::fill,
                     float strokeWidth = 1.0f);

    /**
     * @brief Create a DrawablePath from an SVG string with a specified style.
     *
     * Parses the given SVG string into an XmlElement, then delegates to the
     * XmlElement overload of getDrawablePath() to extract geometry and apply styling.
     *
     * @param svgString    A C‑string containing valid SVG markup.
     * @param colour       Colour to use for stroke or fill.
     * @param elementToAdd The type of SVG element(s) to extract, as defined by ElementType.
     * @param style        The rendering style (stroke, fill, or alternate).
     * @param strokeWidth  Stroke width in pixels (only used for stroke/alternate).
     *
     * @return A std::unique_ptr to a juce::DrawablePath configured with the extracted geometry.
     */
    static const std::unique_ptr<juce::DrawablePath>
    getDrawablePath (const char* svgString,
                     const juce::Colour& colour = juce::Colours::white,
                     ElementType elementToAdd = ElementType::all,
                     PathStyle style = PathStyle::fill,
                     float strokeWidth = 1.0f);

    /**
     * @brief Create a Drawable from an SVG string.
     *
     * Parses the given SVG string into an XmlElement and creates a juce::Drawable
     * from it, suitable for direct rendering.
     *
     * @param svgString A C‑string containing valid SVG markup.
     *
     * @return A std::unique_ptr to a juce::Drawable created from the SVG data.
     *
     * @note Unlike getDrawablePath(), this function returns a generic Drawable
     *       (not specifically a DrawablePath), which may internally be a composite
     *       of multiple shapes.
     */
    static const std::unique_ptr<juce::Drawable> getDrawable (const char* svgString);
#endif // JUCE_MODULE_AVAILABLE_juce_gui_basics
    //==============================================================================
    /** -------------------- FUNCTIONS FOR WRITING SVG ---------------------------*/

    struct Template
    {
        /**
         * @brief Template for a full SVG document declaration.
         *
         * Contains XML declaration, DOCTYPE, and <svg> root element with
         * placeholders for width, height, and inner SVG content.
         *
         * Placeholders:
         * - %%svgWidth%%   → Width of the SVG in pixels.
         * - %%svgHeight%%  → Height of the SVG in pixels.
         * - %%svgString%%  → Inner SVG content (paths, groups, etc.).
         */
        static const juce::String declaration;
        
        /**
         * @brief Template for parsing an SVG path into a juce::Path.
         *
         * Produces a line of code invoking juce::DrawableImage::parseSVGPath().
         *
         * Placeholder:
         * - %%path%% → The SVG path data string.
         */
        static const juce::String parseSVGPath;
        
        /**
         * @brief Template for declaring an array of juce::Path objects.
         *
         * Wraps multiple parsed paths into a juce::Array.
         *
         * Placeholders:
         * - %%varName%% → Name of the array variable.
         * - %%paths%%   → List of path parsing expressions.
         */
        static const juce::String arrayPath;

        struct Ptr
        {
            /**
             * @brief Template for declaring an array of const char* pointers.
             *
             * Typically used to embed multiple SVG string literals.
             *
             * Placeholder:
             * - %%pointers%% → List of string pointer entries.
             */
            static const juce::String chars;
            
            /**
             * @brief Template for embedding a raw SVG string literal.
             *
             * Wraps filtered SVG content inside a raw string literal
             * with a custom delimiter (KuassaSVG).
             *
             * Placeholder:
             * - %%filtered%% → The SVG markup to embed.
             */
            static const juce::String literal;
            
            
            /**
             * @brief Template for creating a Drawable from an SVG string.
             *
             * Produces a code snippet that parses an SVG string and
             * creates a juce::DrawableImage from it.
             *
             * Placeholder:
             * - %%chars%% → The char* string containing SVG markup.
             */
            static const juce::String createFromSVG;
        };

    };


    struct Format
    {
        /*____________________________________________________________________________*/

        struct Ptr
        {
            static const juce::String chars (const juce::String& images);
            static const juce::String literal (juce::XmlElement* parsedXML);
            static const juce::String createFromSVG (const juce::String& chars);
        };

        /**
         * @brief Convert a juce::Path into an SVG path data string.
         *
         * Tokenizes the path string, marks command tokens (A–Z) and numeric tokens,
         * then reassembles them into a properly formatted SVG path string.
         *
         * @param path The juce::Path to convert.
         * @return A juce::String containing the SVG path data, or an empty string
         *         if the path is empty.
         *
         * @note This function ensures that commands and coordinates are separated
         *       with commas and spaces for valid SVG syntax.
         */
        static const juce::String pathToString (const juce::Path& path);
        
        /**
         * @brief Generate an SVG <path> element with stroke styling.
         *
         * Converts the given juce::Path into an SVG <path> element with no fill,
         * a stroke colour, and a stroke width.
         *
         * @param path        The juce::Path to convert.
         * @param colour      The stroke colour.
         * @param strokeWidth The stroke width in pixels.
         * @param pathId      Optional identifier for the path element.
         *
         * @return A juce::String containing the SVG <path> element, or an empty
         *         string if the path is empty.
         */
        static const juce::String stroke (const juce::Path& path,
                                          const juce::Colour& colour,
                                          float strokeWidth,
                                          const juce::String& pathId = juce::String());
        /**
         * @brief Generate an SVG <path> element with fill styling.
         *
         * Converts the given juce::Path into an SVG <path> element with a fill
         * colour and no stroke.
         *
         * @param path   The juce::Path to convert.
         * @param colour The fill colour.
         * @param pathId Optional identifier for the path element.
         *
         * @return A juce::String containing the SVG <path> element, or an empty
         *         string if the path is empty.
         */
        static const juce::String fill (const juce::Path& path,
                                        const juce::Colour& colour,
                                        const juce::String& pathId = juce::String());

        /**
         * @brief Wrap SVG content in a <g> group element.
         *
         * Creates an SVG <g> element containing the provided SVG string, with
         * an optional group identifier.
         *
         * @param svgString The inner SVG content to group.
         * @param groupId   Optional identifier for the group element.
         *
         * @return A juce::String containing the grouped SVG markup.
         */
        static const juce::String group (juce::StringRef svgString,
                                         const juce::String& groupId = juce::String());

        /**
         * @brief Generate an SVG <rect> element from a JUCE rectangle.
         *
         * Converts a juce::Rectangle<int> into an SVG <rect> element with stroke
         * styling and no fill.
         *
         * @param rectangle The rectangle to convert.
         * @param rectId    Identifier for the rect element (converted to a valid ID).
         * @param colour    Stroke colour for the rectangle.
         *
         * @return A juce::String containing the SVG <rect> element.
         *
         * @note The rectangle’s x, y, width, and height are extracted and inserted
         *       into the SVG attributes.
         */
        static const juce::String rect (const juce::Rectangle<int>& rectangle,
                                        const juce::String& rectId = juce::String(),
                                        const juce::Colour& colour = juce::Colours::magenta);

        
        /**
         * @brief Convert SVG elements into an array of path parsing strings.
         *
         * This function inspects the children of the given SVG XmlElement and
         * generates a juce::StringArray of code snippets that parse each shape
         * into a juce::Path using juce::DrawableImage::parseSVGPath().
         *
         * Supported element types:
         * - <path>    → Uses the "d" attribute directly.
         * - <ellipse> → Converted into a juce::Path ellipse, then serialized.
         * - <circle>  → Converted into a juce::Path circle, then serialized.
         * - <rect>    → Converted into a juce::Path rectangle, unless its style
         *               attribute is "fill:none;" (ignored).
         *
         * Each generated string is based on the Template::parseSVGPath format,
         * with %%path%% replaced by the serialized path data.
         *
         * @param svg     Pointer to a juce::XmlElement containing SVG markup.
         *                Must not be nullptr.
         * @param varName Name of the variable to be used in the generated array
         *                (currently unused inside this function, but reserved
         *                for integration with Template::arrayPath).
         *
         * @return A juce::StringArray containing one entry per recognized SVG
         *         element, each entry being a code snippet that parses the
         *         corresponding path.
         *
         * @note The function first flattens group (<g>) elements using
         *       Filter::group() before iterating over child elements.
         * @note Ellipses, circles, and rectangles are converted into juce::Path
         *       objects before being serialized with Format::pathToString().
         */
        static const juce::StringArray getStringArrayPath (juce::XmlElement* svg,
                                                           const juce::String& varName);
    };

    struct File
    {
        /**
         * @brief Generate a complete SVG document string with dimensions and content.
         *
         * This function takes the provided width, height, and inner SVG markup,
         * and substitutes them into the Template::declaration string. The result
         * is a fully‑formed SVG document string that includes the XML declaration,
         * DOCTYPE, <svg> root element, and the supplied inner content.
         *
         * Placeholders replaced:
         * - %%svgWidth%%   → Replaced with the given width (in pixels).
         * - %%svgHeight%%  → Replaced with the given height (in pixels).
         * - %%svgString%%  → Replaced with the provided SVG fragment/content.
         *
         * @param width      The width of the SVG canvas in pixels.
         * @param height     The height of the SVG canvas in pixels.
         * @param svgString  The inner SVG markup to embed inside the <svg> element.
         *
         * @return A juce::String containing the complete SVG document.
         *
         * @note This function is typically used when writing an SVG file to disk,
         *       ensuring that the output is a valid standalone SVG document.
         */
        static const juce::String getStringToWrite (int width, int height, juce::StringRef svgString);
    };

    //==============================================================================
    /**
     * @brief Retrieve the width attribute from an SVG element.
     *
     * Reads the "width" attribute of the given XmlElement and returns it
     * as an integer.
     *
     * @param svg Pointer to a juce::XmlElement representing the root or
     *            fragment of an SVG document. Must not be nullptr.
     *
     * @return The integer value of the "width" attribute. Returns 0 if
     *         the attribute is missing or cannot be parsed.
     */
    static const int getSVGWidth (juce::XmlElement* svg);
    
    /**
     * @brief Retrieve the height attribute from an SVG element.
     *
     * Reads the "height" attribute of the given XmlElement and returns it
     * as an integer.
     *
     * @param svg Pointer to a juce::XmlElement representing the root or
     *            fragment of an SVG document. Must not be nullptr.
     *
     * @return The integer value of the "height" attribute. Returns 0 if
     *         the attribute is missing or cannot be parsed.
     */
    static const int getSVGHeight (juce::XmlElement* svg);
    
    /**
     * @brief Retrieve both width and height attributes from an SVG element.
     *
     * Convenience function that calls getSVGWidth() and getSVGHeight() and
     * returns the results as a std::pair.
     *
     * @param svg Pointer to a juce::XmlElement representing the root or
     *            fragment of an SVG document. Must not be nullptr.
     *
     * @return A std::pair<int, int> containing (width, height).
     *
     * @note This is useful when both dimensions are needed together, e.g.
     *       for scaling or fitting operations.
     */
    static const auto getSVGSize (juce::XmlElement* svg);

    //==============================================================================

    /**
     * @brief A lightweight container for SVG path fragments.
     *
     * The Snapshot class extends juce::StringArray to collect SVG fragments
     * (strokes and fills) generated from juce::Path objects. It is designed
     * to be used when exporting or serializing graphics into SVG format,
     * especially in background threads where thread-safety is critical.
     *
     * Each call to addStroke() or addFill() converts a juce::Path into an
     * SVG string fragment using the Format helpers, and stores it in the
     * underlying StringArray.
     *
     * @note Paths are explicitly copied before conversion to ensure thread-safety.
     *       This avoids race conditions if the original Path is owned or mutated
     *       by GUI components on the message thread.
     */
    struct Snapshot : public juce::StringArray
    {
        /**
         * @brief Default constructor.
         */
        Snapshot() = default;

        /**
         * @brief Add a stroked path to the snapshot.
         *
         * Converts the given juce::Path into an SVG <path> element with stroke
         * attributes, and appends it to the internal StringArray.
         *
         * @param path        The path to be converted. A copy is made internally
         *                    to ensure thread-safety.
         * @param colour      Stroke colour (default: magenta).
         * @param strokeWidth Stroke width in pixels (default: 1.0f).
         * @param strokeId    Identifier string for the SVG element (default: "stroke").
         *
         * @note The path is copied (`juce::Path(path)`) before conversion.
         *       This prevents concurrent access issues if the original path
         *       is modified on another thread (e.g. GUI paint routines).
         */
        void addStroke (const juce::Path& path,
                        juce::Colour colour = juce::Colours::magenta,
                        float strokeWidth = 1.0f,
                        juce::StringRef strokeId = "stroke")
        {
            juce::StringArray::add (Format::stroke (juce::Path (path), colour, strokeWidth, strokeId));
        }

        /**
         * @brief Add a filled path to the snapshot.
         *
         * Converts the given juce::Path into an SVG <path> element with fill
         * attributes, and appends it to the internal StringArray.
         *
         * @param path     The path to be converted. A copy is made internally
         *                 to ensure thread-safety.
         * @param colour   Fill colour (default: yellow).
         * @param fillId   Identifier string for the SVG element (default: "fill").
         *
         * @note The path is copied (`juce::Path(path)`) before conversion.
         *       This prevents concurrent access issues if the original path
         *       is modified on another thread (e.g. GUI paint routines).
         */
        void addFill (const juce::Path& path,
                      juce::Colour colour = juce::Colours::yellow,
                      juce::StringRef fillId = "fill")
        {
            juce::StringArray::add (Format::fill (juce::Path (path), colour, fillId));
        }

        /**
         * @brief Wrap all collected SVG fragments into a grouped <g> element.
         *
         * This method joins all strings currently stored in the Snapshot into
         * a single SVG fragment, separated by newlines, and then wraps them
         * inside an SVG <g> element with the specified group identifier.
         *
         * @param groupId  Identifier string for the SVG group element. This
         *                 will be used as the "id" attribute of the <g> tag.
         *
         * @return A juce::String containing the grouped SVG markup.
         *
         * @note Marked noexcept because it does not throw exceptions.
         * @note This is a convenience method for producing a self‑contained
         *       SVG group from the Snapshot contents, making it easier to
         *       embed the snapshot into larger SVG documents.
         */
        juce::String getGroup (juce::StringRef groupId) const noexcept
        {
            return Format::group (joinIntoString ("\n"), groupId);
        }

    };
};

/**_____________________________END OF NAMESPACE______________________________*/
} /** namespace jreng */
