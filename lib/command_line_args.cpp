#include "command_line_args.hpp"
#include "directoryreader.hpp"

#include "external/CLI11/CLI11.hpp"

std::string CommandLineArgs::_srcsFileTreeName = "srcs_file_tree.bin";
std::string CommandLineArgs::_testsFileTreeName = "tests_file_tree.bin";
std::string CommandLineArgs::_srcsAffectedFileName = "srcs_affected.txt";
std::string CommandLineArgs::_testsAffectedFileName = "tests_affected.txt";
std::string CommandLineArgs::_totalAffectedFileName = "total_affected.txt";
std::string CommandLineArgs::_srcsModifiedFileName = "src_modified.txt";
std::string CommandLineArgs::_testsModifiedFileName = "test_modified.txt";

CommandLineArgs clargs;

CommandLineArgs::CommandLineArgs()
    : _verbal(false), _isNoMain(false), _retCode(0)
{
}

void CommandLineArgs::parseArguments(int argc, char *argv[])
{
    CLI::App app{
        "Description:\n\t"
        "LazyUT detects tests, affected by the changes in the code.\n"};

    std::string rootDir;
    std::string outDirectory;
    std::string inDirectory;

    std::string srcBase;
    std::string testBase;

    std::string exts;
    std::string extra_dependencies;

    // required arguments
    app.add_option("-r,--root", rootDir,
                   "File tree Root directory, every listed file should be "
                   "relative to this")
        ->required();
    app.add_option("-s,--src-dirs", _srcDirectories,
                   "Directories with source files, separated by comma (,),"
                   "relative to Root")
        ->required();
    app.add_option("-t,--test-dirs", _testDirectories,
                   "Directories with test files, separated by comma (,),"
                   "relative to Root")
        ->required();
    app.add_option("-o,--outdir", outDirectory, "Output directory")->required();

    // optional arguments
    app.add_option("-d,--deps", extra_dependencies,
                   "Path to the JSON file with extra dependencies");
    app.add_option("-i,--indir", inDirectory, "Input directory");
    app.add_option("-e,--extensions", exts,
                   "Source files extensions, separated by comma (,)");

    app.add_option("--ignore", _ignoredSubstrings,
                   "Substrings of the ignored paths, separated by comma (,) ");

    app.add_option(
        "--test-base", testBase,
        "Directory relative to which the test source files are displayed");
    app.add_option("--src-base", srcBase,
                   "Directory relative to which the project source files "
                   "are displayed");

    app.add_option("--include-paths", _includePaths,
                   "Extra include paths both for source and test, "
                   "relative to project directory,"
                   "separated by comma (,)");

    app.add_flag("-m,--no-main", _isNoMain,
                 "Don't keep test source file with main() implementation");
    app.add_flag("-v,--verbal", _verbal, "Verbal mode");
    //

    try {
        app.parse(argc, argv);
    }
    catch (const CLI::ParseError &e) {
        _status = Failure;
        _retCode = app.exit(e);
        return;
    }

    if (!exts.empty())
        DirectoryReader::_sourceFileExtensions = split(exts, ",");

    _outDirectory = SplittedPath(outDirectory, SplittedPath::unixSep());
    _inDirectory = SplittedPath(inDirectory, SplittedPath::unixSep());

    if (_inDirectory.empty())
        _inDirectory = _outDirectory;
    if (_outDirectory.empty())
        _outDirectory = _inDirectory;

    _srcsDumpIn = _inDirectory;
    _srcsDumpIn.append(std::string(_srcsFileTreeName));

    _testsDumpIn = _inDirectory;
    _testsDumpIn.append(std::string(_testsFileTreeName));

    _srcsDumpOut = _outDirectory;
    _srcsDumpOut.append(std::string(_srcsFileTreeName));

    _testsDumpOut = _outDirectory;
    _testsDumpOut.append(std::string(_testsFileTreeName));

    _srcsAffected = _outDirectory;
    _srcsAffected.append(std::string(_srcsAffectedFileName));

    _testsAffected = _outDirectory;
    _testsAffected.append(std::string(_testsAffectedFileName));

    _totalAffected = _outDirectory;
    _totalAffected.append(std::string(_totalAffectedFileName));

    _rootDirectory = SplittedPath(rootDir, SplittedPath::unixSep());

    _srcsModified = _outDirectory;
    _srcsModified.append(std::string(_srcsModifiedFileName));
    _testsModified = _outDirectory;
    _testsModified.append(std::string(_testsModifiedFileName));

    if (!srcBase.empty())
        _srcBase = SplittedPath(srcBase, SplittedPath::unixSep());
    else
        _srcBase = _rootDirectory;

    if (!testBase.empty())
        _testBase = SplittedPath(testBase, SplittedPath::unixSep());
    else
        _testBase = _rootDirectory;
    _status = Success;
    _retCode = 0;
}

std::vector< SplittedPath > CommandLineArgs::srcDirectories() const
{
    return splittedPaths(_srcDirectories);
}

std::vector< SplittedPath > CommandLineArgs::testDirectories() const
{
    return splittedPaths(_testDirectories);
}

std::vector< SplittedPath > CommandLineArgs::includePaths() const
{
    return splittedPaths(_includePaths);
}

std::vector< std::string > CommandLineArgs::ignoredSubstrings() const
{
    return split(_ignoredSubstrings, ",");
}

std::vector< SplittedPath >
CommandLineArgs::splittedPaths(const std::string &str)
{
    auto strVector = split(str, ",");
    std::vector< SplittedPath > result;
    result.reserve(strVector.size());

    std::transform(strVector.begin(), strVector.end(),
                   std::back_inserter(result), [](const std::string &s) {
                       return SplittedPath(s, SplittedPath::unixSep());
                   });
    return result;
}
