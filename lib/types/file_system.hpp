#ifndef FILE_SYSTEM_HPP
#define FILE_SYSTEM_HPP

#include "types/file_tree.hpp"

#include <list>

class FileSystem
{
public:
    FileSystem();

    void setProjectDirectory(const SplittedPath &path);
    void installIncludeProjectDir();
    void installExtraDependencies(const std::string &extra_dependencies);
    void installAffectedFiles();

    void analyzePhase();

    FileNode *
    searchInIncludepaths(const SplittedPath &path,
                         const std::list< SplittedPath > &includePaths);
    FileNode *search(const SplittedPath &fullpath);

    FileTree srcTree;
    FileTree testTree;
    FileTree extraDepsTree;

    std::list< FileTree * > _trees;
};

namespace Debug {

void test_analyse_phase(FileSystem &trees);

} // namespace Debug

#endif // FILE_SYSTEM_HPP