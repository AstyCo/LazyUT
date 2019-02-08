#include "types/file_tree.hpp"

#include "extensions/help_functions.hpp"
#include "extensions/flatbuffers_extensions.hpp"

#include "command_line_args.hpp"
#include "directoryreader.hpp"
#include "dependency_analyzer.hpp"
#include "extra_dependency_reader.hpp"

#include "flatbuffers_schemes/file_tree_generated.h"

#include <external/flatbuffers/flatbuffers.h>

#include <sstream>
#include <iostream>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

namespace Debug {

// This function checks whether set "dependencies" and set "dependent by" match
static void test_dependency_dependentyBy_match(FileNode *fnode)
{
    if (fnode == nullptr)
        return;
    for (auto &&file : fnode->_setDependencies) {
        if (file->_setDependentBy.find(fnode) == file->_setDependentBy.end()) {
            std::cerr << "ERROR: FILE " << file->fullPath().joint() << " FNODE "
                      << fnode->fullPath().joint() << std::endl;
            file->_fileTree.print();
            fnode->_fileTree.print();
            assert(false);
        }
    }

    for (auto &&child : fnode->childs())
        test_dependency_dependentyBy_match(child);
}

static void
test_dependencies_recursion_h(FileNode *depNode,
                              const std::set< FileNode * > &totalDependencies,
                              std::set< FileNode * > &testedNodes)
{
    assert(depNode);

    for (auto &&dep : depNode->_setDependencies) {
        assert(totalDependencies.find(dep) != totalDependencies.end());

        // test is recursive also
        if (testedNodes.find(dep) == testedNodes.end()) {
            testedNodes.insert(dep);
            test_dependencies_recursion_h(dep, totalDependencies, testedNodes);
        }
    }
}

// Checks whether dependency set contains the dependencies of dependencies
static void test_dependencies_recursion(FileNode *fnode)
{
    assert(fnode);
    const auto &deps = fnode->_setDependencies;

    std::set< FileNode * > testedNodes;
    testedNodes.insert(fnode);

    for (auto &&file : fnode->_setDependencies)
        test_dependencies_recursion_h(file, deps, testedNodes);

    for (auto &&child : fnode->childs())
        test_dependencies_recursion(child);
}

static void test_dependencies_recursion(FileTree &ftree)
{
    test_dependencies_recursion(ftree.rootNode());
}

static void test_dependency_dependentyBy_match(FileTree &ftree)
{
    test_dependency_dependentyBy_match(ftree.rootNode());
}

void test_analyse_phase(FileTree &tree)
{
    Debug::test_dependency_dependentyBy_match(tree);
    Debug::test_dependencies_recursion(tree);
}

} // namespace Debug

FileRecord::FileRecord(const SplittedPath &path, Type type)
    : _path(path), _type(type), _isHashValid(false)
{
    _path.setUnixSeparator();
}

void FileRecord::calculateHash(const SplittedPath &dir_base)
{
    auto data_pair = readBinaryFile((dir_base + _path).c_str());
    char *data = data_pair.first;
    if (!data) {
        char buff[1000];
        snprintf(buff, sizeof(buff),
                 "LazyUT: Error: File \"%s\" can not be opened",
                 (dir_base + _path).c_str());
        errors() << std::string(buff);

        //        assert(false);
        return;
    }
    MD5 md5((unsigned char *)data, data_pair.second);
    md5.copyResultTo(_hashArray);

    _isHashValid = true;
}

void FileRecord::setHash(const unsigned char *hash)
{
    assert(!_isHashValid);

    copyHashArray(_hashArray, hash);
    _isHashValid = true;
}

std::string FileRecord::hashHex() const
{
    if (!_isHashValid)
        return std::string();

    char buf[33];
    for (int i = 0; i < 16; i++)
        sprintf(buf + i * 2, "%02x", _hashArray[i]);
    buf[32] = 0;

    return std::string(buf);
}

void FileRecord::swapParsedData(FileRecord &record)
{
    _listIncludes.swap(record._listIncludes);
    _setImplements.swap(record._setImplements);
    _setClassDecl.swap(record._setClassDecl);
    _setFuncDecl.swap(record._setFuncDecl);
    _setInheritances.swap(record._setInheritances);
    _listUsingNamespace.swap(record._listUsingNamespace);
}

FileNode::FileNode(const SplittedPath &path, FileRecord::Type type,
                   FileTree &fileTree)
    : _record(path, type), _parent(nullptr), _visited(false),
      _fileTree(fileTree), _flags(Flags::Nothing)
{
}

FileNode::~FileNode()
{
    auto it = _childs.begin();
    while (it != _childs.end()) {
        delete *it;
        ++it;
    }
}

void FileNode::addChild(FileNode *child)
{
    assert(child->parent() == nullptr);

    child->setParent(this);
    _childs.push_back(child);
}

FileNode *FileNode::findOrNewChild(const HashedFileName &hfname,
                                   FileRecord::Type type)
{
    if (FileNode *foundChild = findChild(hfname))
        return foundChild;
    // not found, create new one
    FileNode *newChild;
    if (parent())
        newChild = new FileNode(_record._path + hfname, type, _fileTree);
    else
        newChild = new FileNode(SplittedPath(hfname, SplittedPath::unixSep()),
                                type, _fileTree);
    addChild(newChild);

    return newChild;
}

static void remove_one(std::vector< FileNode * > &container, FileNode *value)
{
    auto it = std::find(container.begin(), container.end(), value);
    if (it == container.end())
        return;
    container.erase(it);
}

void FileNode::removeChild(FileNode *child)
{
    child->setParent(nullptr);

    remove_one(_childs, child);
}

void FileNode::setParent(FileNode *parent) { _parent = parent; }

SplittedPath FileNode::fullPath() const
{
    return _fileTree.rootPath() + path();
}

std::string FileNode::relativeName(const SplittedPath &base) const
{
    return relative_path(fullPath(), base).joint();
}

void FileNode::print(int indent) const
{
    std::string strIndents = makeIndents(indent);

    std::cout << strIndents << "file:" << _record._path.joint();
    if (_record._type == FileRecord::RegularFile)
        std::cout << "\thex:[" << _record.hashHex() << "]";
    std::cout << std::endl;
    //    printInherits(indent);
    //    printInheritsFiles(indent);
    printDependencies(indent);
    printDependentBy(indent);
    printImpls(indent);
    printImplFiles(indent);
    printDecls(indent);
    printFuncImpls(indent);
    printClassImpls(indent);

    auto it = _childs.begin();
    while (it != _childs.end()) {
        (*it)->print(indent + 1);
        ++it;
    }
}

void FileNode::printDecls(int indent) const
{
    std::string strIndents = makeIndents(indent, 2);

    for (auto &decl : _record._setClassDecl)
        std::cout << strIndents << string("class decl: ") << decl.joint()
                  << std::endl;
    for (auto &decl : _record._setFuncDecl)
        std::cout << strIndents << string("function decl: ") << decl.joint()
                  << std::endl;
}

void FileNode::printImpls(int indent) const
{
    std::string strIndents = makeIndents(indent, 2);

    for (auto &impl : _record._setImplements)
        std::cout << strIndents << string("impl: ") << impl.joint()
                  << std::endl;
}

void FileNode::printImplFiles(int indent) const
{
    std::string strIndents = makeIndents(indent, 2);

    for (auto &impl_file : _record._setImplementFiles)
        std::cout << strIndents << string("impl_file: ") << impl_file.joint()
                  << std::endl;
}

void FileNode::printFuncImpls(int indent) const
{
    std::string strIndents = makeIndents(indent, 2);

    for (auto &impl : _record._setFuncImplFiles)
        std::cout << strIndents << string("impl func: ") << impl.joint()
                  << std::endl;
}

void FileNode::printClassImpls(int indent) const
{
    std::string strIndents = makeIndents(indent, 2);

    for (auto &impl : _record._setClassImplFiles)
        std::cout << strIndents << string("impl class: ") << impl.joint()
                  << std::endl;
}

void FileNode::printDependencies(int indent) const
{
    std::string strIndents = makeIndents(indent, 2);

    for (auto &&file : _setDependencies) {
        if (file == this)
            continue; // don't print the file itself
        std::cout << strIndents << string("dependecy: ") << file->name()
                  << std::endl;
    }
}

void FileNode::printDependentBy(int indent) const
{
    std::string strIndents = makeIndents(indent, 2);

    for (auto &&file : _setDependentBy) {
        if (file == this)
            continue; // don't print the file itself
        std::cout << strIndents << string("dependent by: ") << file->name()
                  << std::endl;
    }
}

void FileNode::printInherits(int indent) const
{
    std::string strIndents = makeIndents(indent, 2);

    for (auto &inh : _record._setInheritances)
        std::cout << strIndents << string("inherits: ") << inh.joint()
                  << std::endl;
}

void FileNode::printInheritsFiles(int indent) const
{
    std::string strIndents = makeIndents(indent, 2);

    for (auto &inh : _record._setBaseClassFiles)
        std::cout << strIndents << string("inh_file: ") << inh.joint()
                  << std::endl;
}

bool FileNode::hasRegularFiles() const
{
    if (_record._type == FileRecord::RegularFile)
        return true;
    FileNodeConstIterator it(_childs.begin());
    while (it != _childs.end()) {
        if ((*it)->hasRegularFiles())
            return true;
        ++it;
    }
    return false;
}

void FileNode::destroy()
{
    if (_parent)
        _parent->removeChild(this);
    delete this;
}

void FileNode::installIncludes(const FileTree &fileTree)
{
    for (auto &include_directive : _record._listIncludes) {
        if (FileNode *includedFile =
                fileTree.searchIncludedFile(include_directive, this))
            installExplicitDep(includedFile);
    }
}

void FileNode::installInheritances(const FileTree &fileTree)
{
    for (auto &file_path : _record._setBaseClassFiles) {
        if (FileNode *baseClassFile = fileTree.searchInRoot(file_path))
            installExplicitDep(baseClassFile);
    }
}

void FileNode::installImplements(const FileTree &fileTree)
{
    for (auto &file_path : _record._setImplementFiles) {
        if (FileNode *implementedFile = fileTree.searchInRoot(file_path))
            installExplicitDepBy(implementedFile);
    }
}

void FileNode::installDependencies()
{
    installDepsPrivate(&FileNode::_setDependencies,
                       &FileNode::_setExplicitDependencies);
}

void FileNode::installDependentBy()
{
    installDepsPrivate(&FileNode::_setDependentBy,
                       &FileNode::_setExplicitDependendentBy);
}

void FileNode::clearVisited() { _visited = false; }

FileNode *FileNode::search(const SplittedPath &path)
{
    FileNode *current_dir = this;
    for (auto &fname : path.splitted()) {
        if ((current_dir = current_dir->findChild(fname)))
            continue;
        return nullptr;
    }
    return current_dir;
}

void FileNode::addDependency(FileNode *node)
{
    _setDependencies.insert(node);
    node->_setDependentBy.insert(this);
}

void FileNode::addDependentBy(FileNode *node) { node->addDependency(this); }

void FileNode::installExplicitDep(FileNode *includedNode)
{
    assert(includedNode);
    _setExplicitDependencies.insert(includedNode);
    includedNode->_setExplicitDependendentBy.insert(this);
}

void FileNode::installExplicitDepBy(FileNode *implementedNode)
{
    assert(implementedNode);
    implementedNode->installExplicitDep(this);
}

void FileNode::swapParsedData(FileNode *file)
{
    assert(file);
    _record.swapParsedData(file->_record);
}

bool FileNode::isAffected() const
{
    // if some dependency is affected then this is affected too
    // (this uses affected)
    for (auto &&dep : _setDependencies) {
        if (dep->isThisAffected())
            return true;
    }
    // if some dependent by is affected then this is affected too
    // (affected uses this)
    for (auto &&dep : _setDependentBy) {
        if (dep->isThisAffected())
            return true;
    }

    return false;
}

void FileNode::installDepsPrivate(
    FileNode::SetFileNode FileNode::*getSetDeps,
    const FileNode::SetFileNode FileNode::*getSetExplicitDeps)
{
    _fileTree.clearVisitedR();

    installDepsPrivateR(this, getSetDeps, getSetExplicitDeps);

    // File allways depends on himself
    (this->*getSetDeps).insert(this);
}

template < typename T >
static void append(T &lhs, const T &rhs)
{
    lhs.insert(rhs.begin(), rhs.end());
}

void FileNode::installDepsPrivateR(
    FileNode *node, FileNode::SetFileNode FileNode::*getSetDeps,
    const FileNode::SetFileNode FileNode::*getSetExplicitDeps)
{
    if (node->_visited)
        return;
    node->_visited = true;

    SetFileNode &deps = this->*getSetDeps;
    const SetFileNode &nodeDeps = node->*getSetDeps;

    const SetFileNode &nodeExplicitDeps = node->*getSetExplicitDeps;

    if (!nodeDeps.empty()) {
        append(deps, nodeDeps);
        return;
    }

    append(deps, nodeExplicitDeps);

    for (auto &&explDepsNode : nodeExplicitDeps)
        installDepsPrivateR(explDepsNode, getSetDeps, getSetExplicitDeps);
}

FileNode *FileNode::findChild(const HashedFileName &hfname) const
{
    for (auto child : _childs) {
        if (child->fname() == hfname)
            return child;
    }
    if (hfname.isDotDot())
        return _parent;
    if (hfname.isDot())
        return const_cast< FileNode * >(this);
    return nullptr;
}

FileTree::FileTree() : _rootDirectoryNode(nullptr), _srcParser(*this)
{
    clean();
}

void FileTree::clean()
{
    _state = Clean;
    _rootPath = SplittedPath();
    updateRoot();
}

void FileTree::removeEmptyDirectories()
{
    assert(_state == Filled);
    removeEmptyDirectories(_rootDirectoryNode);
    _state = Filtered;
}

void FileTree::calculateFileHashes()
{
    assert(_state == Filtered);
    calculateFileHashes(_rootDirectoryNode);
    _state = CachesCalculated;
}

void FileTree::parseFiles()
{
    assert(_state == CachesCalculated);
    if (_rootDirectoryNode) {
        parseFilesRecursive(_rootDirectoryNode);
        installModifiedFiles(_rootDirectoryNode);
    }
}

void FileTree::installIncludeNodes()
{
    if (_rootDirectoryNode)
        installIncludeNodesRecursive(*_rootDirectoryNode);
}

void FileTree::installInheritanceNodes()
{
    if (_rootDirectoryNode)
        installInheritanceNodesRecursive(*_rootDirectoryNode);
}

void FileTree::installImplementNodes()
{
    if (_rootDirectoryNode)
        installImplementNodesRecursive(*_rootDirectoryNode);
}

void FileTree::clearVisitedR()
{
    if (_rootDirectoryNode)
        recursiveCall(*_rootDirectoryNode, &FileNode::clearVisited);
}

void FileTree::installDependencies()
{
    if (_rootDirectoryNode)
        recursiveCall(*_rootDirectoryNode, &FileNode::installDependencies);
}

void FileTree::installDependentBy()
{
    if (_rootDirectoryNode)
        recursiveCall(*_rootDirectoryNode, &FileNode::installDependentBy);
}

void FileTree::installAffectedFiles()
{
    assert(_affectedFiles.empty());
    if (_rootDirectoryNode)
        installAffectedFilesRecursive(_rootDirectoryNode);
}

void FileTree::parseModifiedFiles(const FileTree &restored_file_tree)
{
    assert(_state == CachesCalculated);
    compareModifiedFilesRecursive(_rootDirectoryNode,
                                  restored_file_tree._rootDirectoryNode);
    parseModifiedFilesRecursive(_rootDirectoryNode);
}

void FileTree::print() const
{
    if (!_rootDirectoryNode) {
        std::cout << "Empty FileTree" << std::endl;
    }
    else {
        _rootDirectoryNode->print();
    }
}

void FileTree::printAll() const
{
    print();

    std::cout << "AFFECTED SOURCES" << std::endl;
    writeFiles(std::cout, &FileNode::isAffectedSource);

    std::cout << "\nAFFECTED TESTS" << std::endl;
    writeFiles(std::cout, &FileNode::isAffectedTest);

    std::cout << "\nMODIFIED" << std::endl;
    writeFiles(std::cout, &FileNode::isModified);

    std::cout << "\nwrite lazyut files to " << clargs.outDir().joint()
              << std::endl;
}

FileNode *FileTree::addFile(const SplittedPath &relPath)
{
    const std::vector< HashedFileName > &splittedPath = relPath.splitted();
    FileNode *currentNode = _rootDirectoryNode;
    assert(_rootDirectoryNode);
    for (const HashedFileName &fname : splittedPath) {
        FileRecord::Type type =
            ((fname == splittedPath.back()) ? FileRecord::RegularFile
                                            : FileRecord::Directory);
        currentNode = currentNode->findOrNewChild(fname, type);
        assert(currentNode);
    }
    return currentNode;
}

void FileTree::setProjectDirectory(const SplittedPath &path)
{
    _projectDirectory = path;
    _projectDirectory.setUnixSeparator();
}

void FileTree::setRootPath(const SplittedPath &sp)
{
    _rootPath = sp;
    _rootPath.setUnixSeparator();
    updateRoot();
}

FileTree::State FileTree::state() const { return _state; }

void FileTree::setState(const State &state) { _state = state; }

void FileTree::readFiles(const CommandLineArgs &clargs)
{
    readSources(clargs.srcDirectories(), clargs.ignoredSubstrings());
    readTests(clargs.testDirectories(), clargs.ignoredSubstrings());
    _state = Filled;

    removeEmptyDirectories();
    calculateFileHashes();
}

void FileTree::parsePhase(const SplittedPath &spFtreeDump)
{
    FileTree restoredTree;
    FileTreeFunc::deserialize(restoredTree, spFtreeDump);
    if (restoredTree.state() == FileTree::Restored) {
        parseModifiedFiles(restoredTree);
    }
    else {
        // if deserialization failed just parse all
        parseFiles();
    }
}

void FileTree::writeAffectedFiles(const CommandLineArgs &clargs)
{
    // Create directories to put output files to
    boost::filesystem::create_directories(clargs.outDir().joint());

    installAffectedFiles();

    writeFiles(clargs.srcsAffected(), &FileNode::isAffectedSource);
    writeFiles(clargs.testsAffected(), &FileNode::isAffectedTest);
}

static bool containsMain(FileNode *file)
{
    static auto mainPrototype = std::string("main");

    const auto &impls = file->record()._setImplements;
    for (const auto &impl : impls) {
        if (impl.joint() == mainPrototype) {
            return true;
        }
    }
    // doesn't contain
    return false;
}

static void searchTestMainR(FileNode *file,
                            std::vector< FileNode * > &vTestMainFiles)
{
    if (vTestMainFiles.size() > 2)
        return;
    if (file->isRegularFile()) {
        if (containsMain(file) && file->isTestFile())
            vTestMainFiles.push_back(file);
    }
    else { // directory
        for (auto childFile : file->childs())
            searchTestMainR(childFile, vTestMainFiles);
    }
}

static std::vector< FileNode * > searchTestMain(FileTree &testTree)
{
    std::vector< FileNode * > vTestMainFiles;
    vTestMainFiles.reserve(2);
    searchTestMainR(testTree.rootNode(), vTestMainFiles);
    return vTestMainFiles;
}

void FileTree::labelTestMain()
{
    auto filesWithMain = searchTestMain(*this);
    for (FileNode *fileWithMain : filesWithMain)
        fileWithMain->setLabeled();
}

void FileTree::readSources(const std::vector< SplittedPath > &relPaths,
                           const std::vector< std::string > &ignoreSubstrings)
{
    DirectoryReader dr;
    dr._ignore_substrings = ignoreSubstrings;

    for (const SplittedPath &relPath : relPaths)
        dr.readSources(relPath, *this);
}

void FileTree::readTests(const std::vector< SplittedPath > &relPaths,
                         const std::vector< std::string > &ignoreSubstrings)
{
    readSources(relPaths, ignoreSubstrings);
    labelTests(relPaths);
}

void FileTree::labelTests(const std::vector< SplittedPath > &relPaths)
{
    for (const SplittedPath &relPath : relPaths)
        labelTest(relPath);
}

void FileTree::labelTest(const SplittedPath &relPath)
{
    FileNode *node = _rootDirectoryNode->search(relPath);
    if (!node) {
        errors() << "warning: node for path" << relPath.joint()
                 << "doesn't exists -> skipping labeling test file";
        return;
    }
    recursiveCall(*node, &FileNode::setTestFile);
}

void FileTree::analyzePhase()
{
    analyzeNodes();
    propagateDeps();

    DEBUG(Debug::test_analyse_phase(*this));
}

void FileTree::printPaths(std::ostream &os,
                          const std::vector< SplittedPath > &paths) const
{
    for (const SplittedPath &p : paths)
        os << p.jointUnix().c_str() << std::endl;
}

void FileTree::writePaths(const SplittedPath &path,
                          const std::vector< SplittedPath > &paths) const
{
    /* open the file for writing*/
    std::ofstream ofs;
    ofs.open(path.jointOs(), std::ios_base::out);
    if (ofs.is_open())
        printPaths(ofs, paths);
}

static void pushFiles(const FileNode *file,
                      FileNode::BoolProcedureCPtr checkSatisfy,
                      std::vector< SplittedPath > &outFiles)
{
    if (file == nullptr)
        return;

    if ((file->*checkSatisfy)())
        outFiles.push_back(file->path());

    for (auto child : file->childs())
        pushFiles(child, checkSatisfy, outFiles);
}

void FileTree::writeFiles(const SplittedPath &path,
                          FileNode::BoolProcedureCPtr checkSatisfy) const
{
    std::vector< SplittedPath > outFiles;
    pushFiles(_rootDirectoryNode, checkSatisfy, outFiles);
    writePaths(path, outFiles);
}

void FileTree::writeFiles(std::ostream &os,
                          FileNode::BoolProcedureCPtr checkSatisfy) const
{
    std::vector< SplittedPath > outFiles;
    pushFiles(_rootDirectoryNode, checkSatisfy, outFiles);
    printPaths(os, outFiles);
}

void FileTree::installExtraDependencies(const SplittedPath &pathToExtraDeps)
{
    if (pathToExtraDeps.empty())
        return;
    ExtraDependencyReader reader;
    reader.set_extra_dependencies(pathToExtraDeps, *this);
}

void FileTree::addIncludePaths(const std::vector< SplittedPath > &paths)
{
    for (const SplittedPath &path : paths)
        addIncludePath(path);

    if (paths.empty()) // if flags is not set search in project directory
        addIncludePath(SplittedPath("", SplittedPath::unixSep()));
}

void FileTree::addIncludePath(const SplittedPath &path)
{
    if (FileNode *node = rootNode()->search(path))
        _includePaths.push_back(node);
    else
        errors() << "warning: include path" << path.joint() << "not found";
}

std::vector< SplittedPath >
FileTree::affectedFiles(const SplittedPath &spBase,
                        FileNode::FlagsType flags) const
{
    std::vector< SplittedPath > result;
    for (FileNode *node : _affectedFiles) {
        if (!node->checkFlags(flags))
            continue;
        result.push_back(relative_path(node->path(), spBase));
    }
    return result;
}

void FileTree::removeEmptyDirectories(FileNode *node)
{
    if (!node)
        return;
    if (node->record()._type == FileRecord::RegularFile)
        return;

    FileNode::ListFileNode &childs = node->childs();
    FileNode::FileNodeIterator it(childs.begin());

    while (it != childs.end()) {
        removeEmptyDirectories(*it);

        if ((*it)->hasRegularFiles())
            ++it;
        else
            it = childs.erase(it);
    }
}

void FileTree::calculateFileHashes(FileNode *node)
{
    if (!node)
        return;
    if (node->record()._type == FileRecord::RegularFile)
        node->record().calculateHash(_rootPath);

    FileNode::ListFileNode &childs = node->childs();
    FileNode::FileNodeIterator it(childs.begin());

    while (it != childs.end()) {
        calculateFileHashes(*it);

        ++it;
    }
}

void FileTree::parseModifiedFilesRecursive(FileNode *node)
{
    if (node->isRegularFile() && node->isModified())
        _srcParser.parseFile(node);

    for (auto child : node->childs())
        parseModifiedFilesRecursive(child);
}

void FileTree::compareModifiedFilesRecursive(FileNode *node,
                                             FileNode *restored_node)
{
    auto thisChilds = node->childs();
    if (node->isRegularFile()) {
        if (restored_node->isRegularFile() &&
            compareHashArrays(node->record()._hashArray,
                              restored_node->record()._hashArray)) {
            // md5 hash sums match
            node->swapParsedData(restored_node);
        }
        else {
            // md5 hash sums don't match
            node->setModified();
        }
    }
    for (auto child : thisChilds) {
        if (FileNode *restored_child =
                restored_node->findChild(child->fname())) {
            compareModifiedFilesRecursive(child, restored_child);
        }
        else {
            if (clargs.verbal()) {
                std::cout << "this " << node->name() << " child "
                          << child->fname() << " not found" << std::endl;
            }
            installModifiedFiles(child);
        }
    }
}

void FileTree::installModifiedFiles(FileNode *node)
{
    if (node->isRegularFile()) {
        node->setModified();
        return;
    }
    // else, if directory
    for (auto child : node->childs())
        installModifiedFiles(child);
}

void FileTree::parseFilesRecursive(FileNode *node)
{
    if (node->isRegularFile()) {
        _srcParser.parseFile(node);
        return;
    }
    // else, if directory
    for (auto child : node->childs())
        parseFilesRecursive(child);
}

void FileTree::installIncludeNodesRecursive(FileNode &node)
{
    node.installIncludes(*this);
    for (auto &&child : node.childs())
        installIncludeNodesRecursive(*child);
}

void FileTree::installInheritanceNodesRecursive(FileNode &node)
{
    node.installInheritances(*this);
    for (auto &&child : node.childs())
        installInheritanceNodesRecursive(*child);
}

void FileTree::installImplementNodesRecursive(FileNode &node)
{
    node.installImplements(*this);
    for (auto &&child : node.childs())
        installImplementNodesRecursive(*child);
}

void FileTree::installAffectedFilesRecursive(FileNode *node)
{
    if (node->isAffected())
        _affectedFiles.push_back(node);

    for (auto child : node->childs())
        installAffectedFilesRecursive(child);
}

void FileTree::analyzeNodes()
{
    DependencyAnalyzer dep;
    dep.analyze(_rootDirectoryNode);

    installImplementNodes();
    installIncludeNodes();
    installInheritanceNodes();
}

void FileTree::propagateDeps()
{
    installDependencies();
    installDependentBy();
}

void FileTree::recursiveCall(FileNode &node, FileNode::VoidProcedurePtr f)
{
    (node.*f)();
    for (auto &&child : node.childs())
        recursiveCall(*child, f);
}

FileNode *FileTree::searchIncludedFile(const IncludeDirective &id,
                                       FileNode *node) const
{
    assert(node);
    const SplittedPath path(id.filename, SplittedPath::unixSep());
    if (id.isQuotes()) {
        // start from current dir
        if (FileNode *result = searchInCurrentDir(path, node->parent()))
            return result;
        return searchInIncludePaths(path);
    }
    else {
        // start from include paths
        if (FileNode *result = searchInIncludePaths(path))
            return result;
        return searchInCurrentDir(path, node->parent());
    }
}

FileNode *FileTree::searchInCurrentDir(const SplittedPath &path,
                                       FileNode *dir) const
{
    assert(dir && dir->isDirectory());
    if (!dir)
        return nullptr;
    return dir->search(path);
}

FileNode *FileTree::searchInIncludePaths(const SplittedPath &path) const
{
    for (FileNode *node : _includePaths) {
        if (FileNode *foundNode = node->search(path))
            return foundNode;
    }
    return nullptr;
}

FileNode *FileTree::searchInRoot(const SplittedPath &path) const
{
    return searchInCurrentDir(path, _rootDirectoryNode);
}

void FileTree::updateRoot()
{
    delete _rootDirectoryNode;
    _rootDirectoryNode = new FileNode(SplittedPath("", SplittedPath::unixSep()),
                                      FileRecord::Directory, *this);
}

std::string IncludeDirective::toPrint() const
{
    switch (type) {
    case Quotes:
        return '"' + filename + '"';
    case Brackets:
        return '<' + filename + '>';
    }
    return std::string();
}
