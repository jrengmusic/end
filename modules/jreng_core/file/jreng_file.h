namespace jreng
{
/*____________________________________________________________________________*/
struct File
{
    /**
     * @brief Get or create a directory.
     *
     * This function gets the specified child directory from the parent directory.
     * If the child directory does not exist, it creates it.
     *
     * @param parent The parent directory.
     * @param child The name of the child directory to get or create.
     * @return The child directory.
     */
    static const juce::File getOrCreateDirectory (const juce::File& parent,
                                                  const juce::String& child) noexcept
    {
        /** cannot create or get child without name */
        assert (not child.isEmpty());

        auto directory { parent.getChildFile (child) };

        if (not directory.isDirectory())
            directory.createDirectory();

        return directory;
    }

    /**
     * @brief Get the index of a child file in the parent directory.
     *
     * This function searches for a child file with the specified name in the parent directory
     * and returns its index. The search can be recursive and can use a wildcard pattern.
     *
     * @param parent The parent directory.
     * @param childName The name of the child file to search for.
     * @param whatToLook The type of files to look for (e.g., files, directories).
     * @param searchRecursively Whether to search recursively in subdirectories.
     * @param wildCardPattern The wildcard pattern to use for the search.
     * @return The index of the child file if found, or -1 if not found.
     */
    static const int getIndex (const juce::File& parent,
                               const juce::String& childName,
                               int whatToLook,
                               bool searchRecursively = true,
                               const juce::String& wildCardPattern = "*") noexcept
    {
        auto childFiles = parent.findChildFiles (whatToLook | juce::File::ignoreHiddenFiles, searchRecursively, wildCardPattern);

        childFiles.sort();

        for (int index { 0 }; index < childFiles.size(); ++index)
        {
            if (childFiles[index].getFileName().compare (childName) == 0)
                return index;
        }

        return -1;
    }

    /**
     * @brief Get the [Downloads] directory.
     *
     * This function retrieves the path to the user's Downloads directory.
     *
     * @return The path to the user's Downloads directory.
     */
    static const juce::File getDownloadsDirectory() noexcept
    {
        return juce::File::getSpecialLocation (juce::File::SpecialLocationType::userHomeDirectory)
            .getChildFile (IDref::downloads);
    }

    /**
     * @brief Get the [Desktop] directory.
     *
     * This function retrieves the path to the user's Desktop directory.
     *
     * @return The path to the user's Desktop directory.
     */
    static const juce::File getDesktopDirectory() noexcept
    {
        return juce::File::getSpecialLocation (juce::File::SpecialLocationType::userDesktopDirectory);
    }

    /**
     * @brief Get the user application data directory.
     *
     * This function retrieves the path to the user application data directory.
     * On Windows, this is the AppData directory. On macOS, this is the Application Support directory.
     * If a product or company name is provided, it gets or creates a subdirectory with that name.
     *
     * @param productOrCompanyName The name of the product or company to get or create a subdirectory for.
     * @return The path to the user application data directory or the specified subdirectory.
     */
    static const juce::File getUserApplicationDataDirectory (const juce::String& productOrCompanyName = juce::String()) noexcept
    {
        const auto& userApp = []
        {
#if JUCE_WINDOWS /** appData on Windows */
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);

#elif JUCE_MAC /** ~Users/username/Library/Application Support on macOS */
            return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile (IDref::applicationSupport);
#endif
        }();

        if (productOrCompanyName.isEmpty())
            return userApp;

        return File::getOrCreateDirectory (userApp, productOrCompanyName);
    }

    /**
     * @brief Get the common application data directory.
     *
     * This function retrieves the path to the common application data directory.
     * On Windows, this is the AppData directory. On macOS, this is the Application Support directory.
     * If a product or company name is provided, it gets or creates a subdirectory with that name.
     *
     * @param productOrCompanyName The name of the product or company to get or create a subdirectory for.
     * @return The path to the common application data directory or the specified subdirectory.
     */
    static const juce::File getCommonApplicationDataDirectory (const juce::String& productOrCompanyName = juce::String()) noexcept
    {
        const auto& userApp = []
        {
#if JUCE_WINDOWS /** appData on Windows */
            return juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory);

#elif JUCE_MAC /** ~Library/Application Support on macOS */
            return juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory).getChildFile (IDref::applicationSupport);
#endif
        }();

        if (productOrCompanyName.isEmpty())
            return userApp;

        return File::getOrCreateDirectory (userApp, productOrCompanyName);
    }

    /**
     * @brief Get the company directory inside the common application data directory.
     *
     * This function retrieves the path to the company directory inside the common application data directory.
     * On Windows, this is the common AppData directory. On macOS, this is the Application Support directory.
     * If a child directory is provided, it gets or creates a subdirectory with that name.
     *
     * @param childDirectory The name of the child directory to get or create.
     * @return The path to the company directory or the specified subdirectory.
     */
    static const juce::File getCompanyCommonApplicationDataDirectory (const juce::String& childDirectory = juce::String())
    {
        if (childDirectory.isEmpty())
            return getCommonApplicationDataDirectory (companyName);

        return File::getOrCreateDirectory (getCommonApplicationDataDirectory (companyName), childDirectory);
    }

    /**
     * @brief Get the company directory inside the user's Documents or Music directory.
     *
     * This function retrieves the path to the company directory inside the user's Documents directory on Windows
     * or the user's Music directory on macOS.
     *
     * @return The path to the company directory.
     */
    static const juce::File getUserDirectory() noexcept
    {
#if JUCE_WINDOWS
        return getOrCreateDirectory (juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), companyName);
#elif JUCE_MAC
        return getOrCreateDirectory (juce::File::getSpecialLocation (juce::File::userMusicDirectory), companyName);
#endif
    }

    /**
     * @brief Get the user application settings file.
     *
     * This function retrieves the path to the user application settings file. If the file does not exist,
     * it creates a new file with the provided default initial settings.
     *
     * @param defaultInitSettings The default initial settings to use if the file does not exist.
     * @param productOrCompanyName The name of the product or company for which to get the settings file.
     * @param settingsExtension The extension for the settings file.
     * @return The path to the user application settings file.
     */
    static const juce::File getUserApplicationSettings (const juce::ValueTree& defaultInitSettings = juce::ValueTree(),
                                                        const juce::String& productOrCompanyName = projectName,
                                                        juce::StringRef settingsExtension = IDref::settings) noexcept
    {
        auto file { File::getUserApplicationDataDirectory (productOrCompanyName).getChildFile (String::toFileName (productOrCompanyName, settingsExtension)) };

        if (not file.existsAsFile())
        {
            /** first time call must create default init, thus fallback cannot be empty */
            assert (defaultInitSettings.isValid());

            if (defaultInitSettings.isValid())
                if (auto xml { juce::parseXML (defaultInitSettings.toXmlString()) })
                    xml->writeTo (file);
        }

        return file;
    }

    /**
     * @brief Get the common application settings file.
     *
     * This function retrieves the path to the common application settings file.
     * If the file does not exist, it creates a new file with the provided default initial settings.
     *
     * @param defaultInitSettings The default initial settings to use if the file does not exist.
     * @return The path to the common application settings file.
     */
    static const juce::File getCommonApplicationSettings (const juce::ValueTree& defaultInitSettings) noexcept
    {
        auto file { getCommonApplicationDataDirectory (projectName).getChildFile (String::toFileName (projectName, IDtag::settings.toLowerCase())) };

        if (not file.existsAsFile())
        {
            /** first time call must create default init, thus fallback cannot be empty */
            assert (defaultInitSettings.isValid());

            if (defaultInitSettings.isValid())
                if (auto xml { juce::parseXML (defaultInitSettings.toXmlString()) })
                    xml->writeTo (file);
        }

        return file;
    }
    
    static const juce::File getApplicationSettings (const juce::File& settingsPath,
                                                    const juce::ValueTree& defaultInitSettings = juce::ValueTree(),
                                                        juce::StringRef settingsExtension = IDtag::settings.toLowerCase()) noexcept
    {
        auto file { settingsPath.getChildFile (String::toFileName (projectName, settingsExtension)) };

        if (not file.existsAsFile())
        {
            /** first time call must create default init, thus fallback cannot be empty */
            assert (defaultInitSettings.isValid());

            if (defaultInitSettings.isValid())
                if (auto xml { juce::parseXML (defaultInitSettings.toXmlString()) })
                    xml->writeTo (file);
        }

        return file;
    }

    /**
     * @brief Copy missing files from the default directory to the user directory.
     *
     * This function copies files and directories from the default directory to the user directory
     * if they are missing. It can also replace files with newer versions based on the provided version.
     *
     * @param defaultDir The default directory to copy files from.
     * @param userDir The user directory to copy files to.
     * @param currentVersion The current version to compare against for replacing files.
     * @param shouldReplaceWithNewerVersion Whether to replace files with newer versions.
     */
    static void copyMissingFiles (const juce::File& defaultDir,
                                  const juce::File& userDir,
                                  const juce::String& currentVersion,
                                  bool shouldReplaceWithNewerVersion = true)
    {
        if (! defaultDir.isDirectory() || ! userDir.isDirectory())
        {
#if JUCE_DEBUG
            juce::Logger::writeToLog ("One of the directories is not valid!");
#endif
            return;
        }

        for (const juce::File& defaultFile : defaultDir.findChildFiles (juce::File::findFilesAndDirectories, false, "*"))
        {
            auto userFile = userDir.getChildFile (defaultFile.getFileName());

            if (defaultFile.isDirectory())
            {
                if (! userFile.exists())
                {
                    userFile.createDirectory();
#if JUCE_DEBUG
                    juce::Logger::writeToLog ("Created directory " + userFile.getFullPathName());
#endif
                }

                copyMissingFiles (defaultFile, userFile, currentVersion, shouldReplaceWithNewerVersion);
            }
            else
            {
                bool shouldCopy = ! userFile.exists();

                if (shouldReplaceWithNewerVersion)
                {
                    const juce::String defaultVersion = defaultDir.getFileName().fromLastOccurrenceOf ("Ver.", false, true);
                    shouldCopy = shouldCopy || String::isVersionOld (currentVersion, defaultVersion);
                }

                if (shouldCopy)
                {
                    defaultFile.copyFileTo (userFile);
#if JUCE_DEBUG
                    juce::Logger::writeToLog ("Copied " + defaultFile.getFullPathName() + " to " + userFile.getFullPathName());
#endif
                }
            }
        }
    }

    /**
     * @brief Clear the recent files list.
     *
     * This function clears the recent files list by deleting all child elements with the tag name "recent"
     * from the user application data settings file.
     */
    static const void clearRecentFilesList()
    {
        if (auto settings { File::getUserApplicationDataDirectory() };
            settings.existsAsFile())
        {
            if (auto xml { juce::parseXML (settings) })
            {
                xml->deleteAllChildElementsWithTagName (IDtag::recent);

                xml->writeTo (settings);
            }
        }
    }
};

/**_____________________________END OF NAMESPACE______________________________*/
}// namespace jreng
