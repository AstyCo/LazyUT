#ifndef DIRECTORY_READER_HPP
#define DIRECTORY_READER_HPP

#include "types/file_tree.hpp"

#include <iostream>
#include <vector>

#include <memory>

class DirectoryIteratorImpl;
class DirectoryIterator
{
public:
    explicit DirectoryIterator(const SplittedPath &path = SplittedPath());
    DirectoryIterator(const DirectoryIterator &o);

    DirectoryIterator &operator++();   // prefix increment
    DirectoryIterator operator++(int); // postfix increment

    bool operator==(const DirectoryIterator &o) const;
    bool operator!=(const DirectoryIterator &o) const { return !(*this == o); }
    const SplittedPath &operator*() const;

private:
    std::unique_ptr< DirectoryIteratorImpl > _impl;
};

class DirectoryReader
{
public:
    using StringVector = std::vector< std::string >;

    static StringVector _sourceFileExtensions;
    static StringVector _ignore_substrings;

    void readSources(const SplittedPath &relPath, FileNode *parent);
    void readSources(const SplittedPath &relPath, FileTree &filetree);

    void setTestPatterns(const StringVector &patterns);

private:
    void removeEmptyDirectories(FileTree &fileTree);

    bool isSourceFile(const SplittedPath &sp) const; /// TODO use
    bool isIgnored(const SplittedPath &sp) const;
    bool isIgnoredOsSep(const std::string &path) const;

    FileRecord::Type getFileType(const SplittedPath &sp) const;

private:
    StringVector _testPatterns;
};

#endif // DIRECTORY_READER_HPP
